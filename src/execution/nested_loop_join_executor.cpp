//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_loop_join_executor.cpp
//
// Identification: src/execution/nested_loop_join_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_loop_join_executor.h"
#include <utility>
#include <vector>
#include "binder/table_ref/bound_join_ref.h"
#include "common/exception.h"
#include "common/logger.h"
#include "storage/table/tuple.h"
#include "type/value.h"
#include "type/value_factory.h"

namespace bustub {

NestedLoopJoinExecutor::NestedLoopJoinExecutor(ExecutorContext *exec_ctx, const NestedLoopJoinPlanNode *plan,
                                               std::unique_ptr<AbstractExecutor> &&left_executor,
                                               std::unique_ptr<AbstractExecutor> &&right_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      left_executor_(std::move(left_executor)),
      right_executor_(std::move(right_executor)) {
  if (!(plan->GetJoinType() == JoinType::LEFT || plan->GetJoinType() == JoinType::INNER)) {
    // Note for 2023 Fall: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

void NestedLoopJoinExecutor::Init() {
  left_executor_->Init();
  right_executor_->Init();

  finish_scan_inner_table_ = true;
  left_tuple_ = Tuple{};
  left_join_tuples_ = std::vector<Tuple>{};
  left_join_tuples_index_ = 0;
  left_tuple_match_any_right_tuple_ = false;
}

auto NestedLoopJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  auto filter_expr = plan_->Predicate();

  Tuple right_tuple{};
  while (!finish_scan_inner_table_ || left_executor_->Next(&left_tuple_, rid)) {
    if (finish_scan_inner_table_) {
      left_tuple_match_any_right_tuple_ = false;
    }

    finish_scan_inner_table_ = false;

    while (right_executor_->Next(&right_tuple, rid)) {
      auto status = filter_expr->EvaluateJoin(&left_tuple_, plan_->GetLeftPlan()->OutputSchema(), &right_tuple,
                                              plan_->GetRightPlan()->OutputSchema());

      if (status.IsNull() || !status.GetAs<bool>()) {
        continue;
      }

      std::vector<Value> values;
      for (int i = 0; i < static_cast<int>(plan_->GetLeftPlan()->OutputSchema().GetColumnCount()); i++) {
        values.emplace_back(left_tuple_.GetValue(&plan_->GetLeftPlan()->OutputSchema(), i));
      }

      for (int i = 0; i < static_cast<int>(plan_->GetRightPlan()->OutputSchema().GetColumnCount()); i++) {
        values.emplace_back(right_tuple.GetValue(&plan_->GetRightPlan()->OutputSchema(), i));
      }

      *tuple = Tuple{values, &GetOutputSchema()};
      left_tuple_match_any_right_tuple_ = true;

      return true;
    }

    if (!left_tuple_match_any_right_tuple_ && plan_->GetJoinType() == JoinType::LEFT) {
      left_join_tuples_.emplace_back(left_tuple_);
    }

    finish_scan_inner_table_ = true;
    right_executor_->Init();
  }

  if (plan_->join_type_ == JoinType::LEFT) {
    while (left_join_tuples_index_ < static_cast<int>(left_join_tuples_.size())) {
      std::vector<Value> values;

      for (int i = 0; i < static_cast<int>(plan_->GetLeftPlan()->OutputSchema().GetColumnCount()); i++) {
        values.emplace_back(
            left_join_tuples_[left_join_tuples_index_].GetValue(&plan_->GetLeftPlan()->OutputSchema(), i));
      }

      for (const auto &column : plan_->GetRightPlan()->OutputSchema().GetColumns()) {
        values.emplace_back(ValueFactory::GetNullValueByType(column.GetType()));
      }

      *tuple = Tuple{values, &GetOutputSchema()};
      left_join_tuples_index_++;

      return true;
    }
  }

  return false;
}

}  // namespace bustub
