//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// delete_executor.cpp
//
// Identification: src/execution/delete_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>
#include <utility>

#include "execution/executors/delete_executor.h"

namespace bustub {

DeleteExecutor::DeleteExecutor(ExecutorContext *exec_ctx, const DeletePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void DeleteExecutor::Init() {
  child_executor_->Init();
  auto catalog = exec_ctx_->GetCatalog();

  table_info_ = catalog->GetTable(plan_->GetTableOid());
  indexes_info_ = catalog->GetTableIndexes(table_info_->name_);

  inserted_rows_count_ = 0;
  is_finished_ = false;
}

auto DeleteExecutor::Next([[maybe_unused]] Tuple *tuple, RID *rid) -> bool {
  if (is_finished_) {
    return false;
  }

  //* first call the child executor and read the tuples
  Tuple child_tuple{};
  while (child_executor_->Next(&child_tuple, rid)) {
    inserted_rows_count_++;

    //* remove the old tuples from the table (logically)
    table_info_->table_->UpdateTupleMeta(TupleMeta{0, true}, *rid);

    //* update any related indexes
    for (const auto &index_info : indexes_info_) {
      auto key_tuple =
          child_tuple.KeyFromTuple(table_info_->schema_, index_info->key_schema_, index_info->index_->GetKeyAttrs());
      index_info->index_->DeleteEntry(key_tuple, *rid, exec_ctx_->GetTransaction());
    }
  }

  //* return the number of effected tuples
  is_finished_ = true;
  auto value = Value{TypeId::INTEGER, inserted_rows_count_};
  *tuple = Tuple{std::vector<Value>{value}, &GetOutputSchema()};

  return true;
}

}  // namespace bustub
