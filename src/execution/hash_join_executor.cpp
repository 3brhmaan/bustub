//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// hash_join_executor.cpp
//
// Identification: src/execution/hash_join_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/hash_join_executor.h"
#include <memory>
#include <utility>
#include <vector>
#include "binder/table_ref/bound_join_ref.h"
#include "common/logger.h"
#include "execution/plans/hash_join_plan.h"
#include "storage/table/tuple.h"
#include "type/value.h"
#include "type/value_factory.h"

namespace bustub {

HashJoinExecutor::HashJoinExecutor(ExecutorContext *exec_ctx, const HashJoinPlanNode *plan,
                                   std::unique_ptr<AbstractExecutor> &&left_child,
                                   std::unique_ptr<AbstractExecutor> &&right_child)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      left_child_(std::move(left_child)),
      right_child_(std::move(right_child)) {
  if (!(plan->GetJoinType() == JoinType::LEFT || plan->GetJoinType() == JoinType::INNER)) {
    // Note for Fall 2024: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

void HashJoinExecutor::Init() {
  left_child_->Init();
  right_child_->Init();
  ht_ = std::make_unique<SimpleJoinHashTable>();
  right_tuple_ = Tuple{};
  left_tuples_idx_ = 0;
  left_join_tuples_idx_ = 0;
  finish_left_tuples_ = true;

  Tuple left_tuple{};
  RID rid{-1};
  while (left_child_->Next(&left_tuple, &rid)) {
    HashJoinKey hash_key{};
    for (auto &expr : plan_->LeftJoinKeyExpressions()) {
      hash_key.attributes_.emplace_back(expr->Evaluate(&left_tuple, left_child_->GetOutputSchema()));
    }

    ht_->Insert(hash_key, left_tuple);
  }

  start_iter_ = std::make_unique<SimpleJoinHashTable::Iterator>(ht_->Begin());
  end_iter_ = std::make_unique<SimpleJoinHashTable::Iterator>(ht_->End());
}

auto HashJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  //! handle left join
  while (!finish_left_tuples_ || right_child_->Next(&right_tuple_, rid)) {
    HashJoinKey hash_key{};
    for (auto &expr : plan_->RightJoinKeyExpressions()) {
      hash_key.attributes_.emplace_back(expr->Evaluate(&right_tuple_, right_child_->GetOutputSchema()));
    }

    finish_left_tuples_ = false;

    if (ht_->Contains(hash_key)) {
      matched_left_tuples_ = ht_->Get(hash_key);

      while (left_tuples_idx_ < static_cast<int>(matched_left_tuples_.size())) {
        auto left_tuple = matched_left_tuples_[left_tuples_idx_];

        std::vector<Value> values;
        for (int i = 0; i < static_cast<int>(plan_->GetLeftPlan()->OutputSchema().GetColumnCount()); i++) {
          values.emplace_back(left_tuple.GetValue(&plan_->GetLeftPlan()->OutputSchema(), i));
        }

        for (int i = 0; i < static_cast<int>(plan_->GetRightPlan()->OutputSchema().GetColumnCount()); i++) {
          values.emplace_back(right_tuple_.GetValue(&plan_->GetRightPlan()->OutputSchema(), i));
        }

        *tuple = Tuple{values, &GetOutputSchema()};

        left_tuples_idx_++;
        if (left_tuples_idx_ == static_cast<int>(matched_left_tuples_.size())) {
          finish_left_tuples_ = true;
          left_tuples_idx_ = 0;
          matched_left_tuples_.clear();
        }

        return true;
      }
    }

    finish_left_tuples_ = true;
  }

  if (plan_->GetJoinType() == JoinType::LEFT) {
    while ((*start_iter_) != (*end_iter_)) {
      if (ht_->matched_hash_keys_.count(start_iter_->Key()) == 0U) {
        auto left_tuples = start_iter_->Val();

        for (const auto &t : left_tuples) {
          left_join_tuples_.push_back(t);
        }
      }

      ++(*start_iter_);
    }

    while (left_join_tuples_idx_ < static_cast<int>(left_join_tuples_.size())) {
      std::vector<Value> values;
      for (int i = 0; i < static_cast<int>(plan_->GetLeftPlan()->OutputSchema().GetColumnCount()); i++) {
        values.emplace_back(
            left_join_tuples_[left_join_tuples_idx_].GetValue(&plan_->GetLeftPlan()->OutputSchema(), i));
      }

      for (const auto &column : plan_->GetRightPlan()->OutputSchema().GetColumns()) {
        values.emplace_back(ValueFactory::GetNullValueByType(column.GetType()));
      }

      *tuple = Tuple{values, &GetOutputSchema()};

      left_join_tuples_idx_++;

      return true;
    }
  }

  return false;
}

}  // namespace bustub
