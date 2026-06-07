//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_index_join_executor.cpp
//
// Identification: src/execution/nested_index_join_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_index_join_executor.h"
#include <utility>
#include <vector>
#include "binder/table_ref/bound_join_ref.h"
#include "common/logger.h"
#include "common/rid.h"
#include "type/value_factory.h"

namespace bustub {

NestIndexJoinExecutor::NestIndexJoinExecutor(ExecutorContext *exec_ctx, const NestedIndexJoinPlanNode *plan,
                                             std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {
  if (!(plan->GetJoinType() == JoinType::LEFT || plan->GetJoinType() == JoinType::INNER)) {
    // Note for 2023 Spring: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

void NestIndexJoinExecutor::Init() {
  child_executor_->Init();

  auto catalog = exec_ctx_->GetCatalog();
  inner_index_info_ = catalog->GetIndex(plan_->GetIndexOid());
  inner_table_info_ = catalog->GetTable(plan_->GetInnerTableOid());
}

auto NestIndexJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  Tuple outer_tuple{};
  while (child_executor_->Next(&outer_tuple, rid)) {
    auto outer_tuple_key_value = plan_->KeyPredicate()->Evaluate(&outer_tuple, child_executor_->GetOutputSchema());

    Tuple outer_key_tuple{std::vector<Value>{outer_tuple_key_value}, &inner_index_info_->key_schema_};
    std::vector<RID> result;
    inner_index_info_->index_->ScanKey(outer_key_tuple, &result, exec_ctx_->GetTransaction());

    if (!result.empty()) {
      auto [tuple_meta, inner_tuple] = inner_table_info_->table_->GetTuple(result[0]);
      if (!tuple_meta.is_deleted_) {
        std::vector<Value> values;
        values.reserve(GetOutputSchema().GetColumnCount());
        for (int i = 0; i < static_cast<int>(child_executor_->GetOutputSchema().GetColumnCount()); i++) {
          values.emplace_back(outer_tuple.GetValue(&child_executor_->GetOutputSchema(), i));
        }

        for (int i = 0; i < static_cast<int>(inner_table_info_->schema_.GetColumnCount()); i++) {
          values.emplace_back(inner_tuple.GetValue(&inner_table_info_->schema_, i));
        }

        *tuple = Tuple{values, &GetOutputSchema()};

        return true;
      }
    }

    if (result.empty() && plan_->GetJoinType() == JoinType::LEFT) {
      std::vector<Value> values;
      values.reserve(GetOutputSchema().GetColumnCount());
      for (int i = 0; i < static_cast<int>(child_executor_->GetOutputSchema().GetColumnCount()); i++) {
        values.emplace_back(outer_tuple.GetValue(&child_executor_->GetOutputSchema(), i));
      }

      for (const auto &column : inner_table_info_->schema_.GetColumns()) {
        values.emplace_back(ValueFactory::GetNullValueByType(column.GetType()));
      }

      *tuple = Tuple{values, &GetOutputSchema()};

      return true;
    }
  }

  return false;
}

}  // namespace bustub
