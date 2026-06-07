//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// update_executor.cpp
//
// Identification: src/execution/update_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>
#include <utility>
#include <vector>
#include "fmt/core.h"
#include "storage/table/tuple.h"
#include "type/type_id.h"
#include "type/vector_type.h"

#include "execution/executors/update_executor.h"

namespace bustub {

UpdateExecutor::UpdateExecutor(ExecutorContext *exec_ctx, const UpdatePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {
  // As of Fall 2022, you DON'T need to implement update executor to have perfect score in project 3 / project 4.
}

void UpdateExecutor::Init() {
  child_executor_->Init();
  auto catalog = exec_ctx_->GetCatalog();

  table_info_ = catalog->GetTable(plan_->GetTableOid());
  indexes_info_ = catalog->GetTableIndexes(table_info_->name_);

  inserted_rows_count_ = 0;
  is_finished_ = false;
}

auto UpdateExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) -> bool {
  if (is_finished_) {
    return false;
  }

  //* first call the child executor and read the tuples
  Tuple child_tuple{};
  while (child_executor_->Next(&child_tuple, rid)) {
    inserted_rows_count_++;

    //* update the tuples using the expression
    std::vector<Value> values;
    for (const auto &expr : plan_->target_expressions_) {
      values.push_back(expr->Evaluate(&child_tuple, table_info_->schema_));
    }

    Tuple updated_tuple{values, &table_info_->schema_};

    //* remove the old tuples from the table (logically)
    table_info_->table_->UpdateTupleMeta(TupleMeta{0, true}, *rid);

    //* re-insert the updated tuples
    auto inserted_rid =
        table_info_->table_->InsertTuple(TupleMeta{0, false}, updated_tuple, exec_ctx_->GetLockManager(),
                                         exec_ctx_->GetTransaction(), table_info_->oid_);

    //* update any related indexes
    for (const auto &index_info : indexes_info_) {
      //* remove the old key from the index
      auto old_key_tuple =
          child_tuple.KeyFromTuple(table_info_->schema_, index_info->key_schema_, index_info->index_->GetKeyAttrs());
      index_info->index_->DeleteEntry(old_key_tuple, *rid, exec_ctx_->GetTransaction());

      //* re-insert the new key into the index
      auto new_key_tuple =
          updated_tuple.KeyFromTuple(table_info_->schema_, index_info->key_schema_, index_info->index_->GetKeyAttrs());
      index_info->index_->InsertEntry(new_key_tuple, inserted_rid.value(), exec_ctx_->GetTransaction());
    }
  }

  //* return the number of effected tuples
  is_finished_ = true;
  auto value = Value{TypeId::INTEGER, inserted_rows_count_};
  *tuple = Tuple{std::vector<Value>{value}, &GetOutputSchema()};

  return true;
}

}  // namespace bustub
