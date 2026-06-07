//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// insert_executor.cpp
//
// Identification: src/execution/insert_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>
#include <utility>
#include "fmt/core.h"
#include "storage/table/tuple.h"
#include "type/type_id.h"

#include "execution/executors/insert_executor.h"

namespace bustub {

InsertExecutor::InsertExecutor(ExecutorContext *exec_ctx, const InsertPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void InsertExecutor::Init() {
  child_executor_->Init();
  auto catalog = exec_ctx_->GetCatalog();

  table_info_ = catalog->GetTable(plan_->GetTableOid());
  indexes_info_ = catalog->GetTableIndexes(table_info_->name_);

  inserted_rows_count_ = 0;
  is_finished_ = false;
}

auto InsertExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) -> bool {
  if (is_finished_) {
    return false;
  }

  Tuple child_tuple{};
  TupleMeta meta{0, false};

  while (child_executor_->Next(&child_tuple, rid)) {
    inserted_rows_count_++;

    //* insert tuples into table
    auto inserted_rid = table_info_->table_->InsertTuple(meta, child_tuple, exec_ctx_->GetLockManager(),
                                                         exec_ctx_->GetTransaction(), table_info_->oid_);

    //* update the indexs related to this table with the data
    for (const auto &index_info : indexes_info_) {
      //* get the key related to this index from the tuple and insert it into the index
      auto key_tuple =
          child_tuple.KeyFromTuple(table_info_->schema_, index_info->key_schema_, index_info->index_->GetKeyAttrs());

      index_info->index_->InsertEntry(key_tuple, inserted_rid.value(), exec_ctx_->GetTransaction());
    }
  }

  Value value{TypeId::INTEGER, inserted_rows_count_};
  *tuple = Tuple{std::vector<Value>{value}, &GetOutputSchema()};

  is_finished_ = true;
  return true;
}

}  // namespace bustub
