//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// aggregation_executor.cpp
//
// Identification: src/execution/aggregation_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "common/logger.h"
#include "fmt/core.h"
#include "storage/table/tuple.h"
#include "type/value.h"
#include "type/value_factory.h"

#include "execution/executors/aggregation_executor.h"

namespace bustub {

AggregationExecutor::AggregationExecutor(ExecutorContext *exec_ctx, const AggregationPlanNode *plan,
                                         std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {
  is_table_empty_ = true;
  is_finish_ = false;
}

void AggregationExecutor::Init() {
  child_executor_->Init();

  hash_table_ = std::make_unique<SimpleAggregationHashTable>(plan_->GetAggregates(), plan_->GetAggregateTypes());
  Tuple child_tuple{};
  RID rid{5};

  while (child_executor_->Next(&child_tuple, &rid)) {
    auto aggregate_key = MakeAggregateKey(&child_tuple);
    auto aggregate_value = MakeAggregateValue(&child_tuple);

    hash_table_->InsertCombine(aggregate_key, aggregate_value);
    is_table_empty_ = false;
  }

  start_iter_ = std::make_unique<SimpleAggregationHashTable::Iterator>(hash_table_->Begin());
  end_iter_ = std::make_unique<SimpleAggregationHashTable::Iterator>(hash_table_->End());
}

auto AggregationExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  while ((*start_iter_) != (*end_iter_)) {
    //* The output schema consists of the group-by columns followed by the aggregation columns.
    std::vector<Value> values;
    for (const auto &it : start_iter_->Key().group_bys_) {
      values.emplace_back(it);
    }
    for (const auto &it : start_iter_->Val().aggregates_) {
      values.emplace_back(it);
    }

    *tuple = Tuple{values, &GetOutputSchema()};

    ++(*start_iter_);

    return true;
  }

  //* When performing aggregation on an empty table, CountStarAggregate should return zero and all other aggregate types
  //* should return integer_null
  if (!is_finish_ && is_table_empty_) {
    //* if the query have GROUP BY Clasue and the Table is empty then do nothing and return false
    if (!plan_->group_bys_.empty()) {
      return false;
    }

    std::vector<Value> values;

    for (auto type : plan_->GetAggregateTypes()) {
      switch (type) {
        case AggregationType::CountStarAggregate:
          values.emplace_back(ValueFactory::GetIntegerValue(0));
          break;
        case AggregationType::CountAggregate:
        case AggregationType::SumAggregate:
        case AggregationType::MinAggregate:
        case AggregationType::MaxAggregate:
          values.emplace_back(ValueFactory::GetNullValueByType(TypeId::INTEGER));
          break;
      }
    }

    *tuple = Tuple{values, &GetOutputSchema()};

    is_finish_ = true;
    return true;
  }

  return false;
}

auto AggregationExecutor::GetChildExecutor() const -> const AbstractExecutor * { return child_executor_.get(); }

}  // namespace bustub
