//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// seq_scan_executor.cpp
//
// Identification: src/execution/seq_scan_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/seq_scan_executor.h"
#include <cstdint>
#include "fmt/core.h"
#include "storage/table/table_iterator.h"
#include "type/type_id.h"

namespace bustub {

SeqScanExecutor::SeqScanExecutor(ExecutorContext *exec_ctx, const SeqScanPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan) {}

void SeqScanExecutor::Init() {
  auto catalog = exec_ctx_->GetCatalog();
  table_info_ = catalog->GetTable(plan_->GetTableOid());

  table_iterator_ = std::make_unique<TableIterator>(table_info_->table_->MakeIterator());
}

auto SeqScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  while (!table_iterator_->IsEnd()) {
    auto [meta, current_tuple] = table_iterator_->GetTuple();
    auto value = plan_->filter_predicate_ != nullptr
                     ? plan_->filter_predicate_->Evaluate(&current_tuple, table_info_->schema_)
                     : Value{TypeId::BOOLEAN, static_cast<int8_t>(true)};

    //* check if the tuple isn't deleted and pass the filter
    if (!meta.is_deleted_ && !value.IsNull() && value.GetAs<bool>()) {
      *rid = table_iterator_->GetRID();
      *tuple = current_tuple;

      ++(*table_iterator_);
      return true;
    }
    ++(*table_iterator_);
  }

  return false;
}

}  // namespace bustub
