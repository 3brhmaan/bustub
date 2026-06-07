//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_scan_executor.h
//
// Identification: src/include/execution/executors/index_scan_executor.h
//
// Copyright (c) 2015-20, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "catalog/catalog.h"
#include "common/rid.h"
#include "execution/executor_context.h"
#include "execution/executors/abstract_executor.h"
#include "execution/plans/index_scan_plan.h"
#include "storage/index/b_plus_tree_index.h"
#include "storage/index/index_iterator.h"
#include "storage/table/tuple.h"

namespace bustub {

/**
 * IndexScanExecutor executes an index scan over a table.
 */

class IndexScanExecutor : public AbstractExecutor {
 public:
  /**
   * Creates a new index scan executor.
   * @param exec_ctx the executor context
   * @param plan the index scan plan to be executed
   */
  IndexScanExecutor(ExecutorContext *exec_ctx, const IndexScanPlanNode *plan);
  ~IndexScanExecutor() override;

  auto GetOutputSchema() const -> const Schema & override { return plan_->OutputSchema(); }

  void Init() override;

  auto Next(Tuple *tuple, RID *rid) -> bool override;

 private:
  /** The index scan plan node to be executed. */
  const IndexScanPlanNode *plan_;

  std::shared_ptr<IndexInfo> index_info_;
  std::shared_ptr<TableInfo> table_info_;

  BPlusTreeIndexForTwoIntegerColumn *tree_index_;
  BPlusTreeIndexIteratorForTwoIntegerColumn *iter_;
  int prediction_keys_index_;

  // std::unique_ptr<BPlusTreeIndexForTwoIntegerColumn> tree_index_;
  // std::unique_ptr<BPlusTreeIndexIteratorForTwoIntegerColumn> iter_;
};
}  // namespace bustub
