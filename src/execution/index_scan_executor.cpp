//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_scan_executor.cpp
//
// Identification: src/execution/index_scan_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include "execution/executors/index_scan_executor.h"
#include <cstddef>
#include <cstdio>
#include <memory>
#include <utility>
#include <vector>
#include "catalog/catalog.h"
#include "common/logger.h"
#include "common/macros.h"
#include "concurrency/transaction.h"
#include "execution/expressions/constant_value_expression.h"
#include "fmt/core.h"
#include "storage/index/b_plus_tree_index.h"
#include "type/value.h"

namespace bustub {
IndexScanExecutor::IndexScanExecutor(ExecutorContext *exec_ctx, const IndexScanPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan) {}

void IndexScanExecutor::Init() {
  auto catalog = exec_ctx_->GetCatalog();
  index_info_ = catalog->GetIndex(plan_->GetIndexOid());
  table_info_ = catalog->GetTable(plan_->table_oid_);
  tree_index_ = dynamic_cast<BPlusTreeIndexForTwoIntegerColumn *>(index_info_->index_.get());
  prediction_keys_index_ = 0;

  if (plan_->filter_predicate_ == nullptr) {
    iter_ = new BPlusTreeIndexIteratorForTwoIntegerColumn(tree_index_->GetBeginIterator());
    // iter_ = std::make_unique<BPlusTreeIndexIteratorForTwoIntegerColumn>((tree_index_->GetBeginIterator()));
  }
}

auto IndexScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (plan_->filter_predicate_ == nullptr) {
    //* handle order by CALUSE

    while (!iter_->IsEnd()) {
      auto [_, tuple_rid] = **iter_;
      auto [tuple_meta, tuple_data] = table_info_->table_->GetTuple(tuple_rid);

      if (!tuple_meta.is_deleted_) {
        *tuple = tuple_data;
        *rid = tuple_rid;

        ++(*iter_);
        return true;
      }

      ++(*iter_);
    }

    return false;
  }
  //* handle lookups / where clause
  //* this executor returns one tuple per point lookup if it exists and do not emit tuples that are deleted.
  //* there could be more than one predicition key but they belong to the same column

  while (prediction_keys_index_ < static_cast<int>(plan_->pred_keys_.size())) {
    auto &expression = plan_->pred_keys_[prediction_keys_index_];

    auto fake_tuple = Tuple{RID{-1}};
    auto fake_schema = Schema{std::vector<Column>{}};

    std::vector<Value> values;
    values.push_back(expression->Evaluate(&fake_tuple, fake_schema));

    Tuple key_tuple{values, &index_info_->key_schema_};
    std::vector<RID> result;

    tree_index_->ScanKey(key_tuple, &result, exec_ctx_->GetTransaction());

    if (!result.empty()) {
      auto [tuple_meta, tuple_data] = table_info_->table_->GetTuple(result[0]);

      if (!tuple_meta.is_deleted_) {
        *tuple = tuple_data;
        *rid = result[0];

        prediction_keys_index_++;
        return true;
      }
    }

    prediction_keys_index_++;
  }

  return false;
}

IndexScanExecutor::~IndexScanExecutor() {
  if (plan_->filter_predicate_ == nullptr) {
    delete iter_;
  }
}

}  // namespace bustub
