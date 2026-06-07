//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// external_merge_sort_executor.cpp
//
// Identification: src/execution/external_merge_sort_executor.cpp
//
// Copyright (c) 2015-2024, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/external_merge_sort_executor.h"
#include <algorithm>
#include <iostream>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "buffer/buffer_pool_manager.h"
#include "common/config.h"
#include "common/logger.h"
#include "common/macros.h"
#include "execution/execution_common.h"
#include "execution/plans/sort_plan.h"
#include "storage/table/tuple.h"

namespace bustub {

template <size_t K>
ExternalMergeSortExecutor<K>::ExternalMergeSortExecutor(ExecutorContext *exec_ctx, const SortPlanNode *plan,
                                                        std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), cmp_(plan->GetOrderBy()), child_executor_(std::move(child_executor)) {}

template <size_t K>
void ExternalMergeSortExecutor<K>::HandleSortingTuples(int max_size, BufferPoolManager *bpm) {
  std::vector<SortEntry> entries;
  entries.reserve(max_size);

  Tuple child_tuple;
  RID rid{0};

  auto flush_to_disk = [&]() {
    if (entries.empty()) {
      return;
    }

    auto page_id = bpm->NewPage();
    auto page_guard = bpm->WritePage(page_id);
    auto sort_page = page_guard.AsMut<SortPage>();

    sort_page->Init(&plan_->OutputSchema());
    std::sort(entries.begin(), entries.end(), TupleComparator{plan_->GetOrderBy()});
    for (auto [_, t] : entries) {
      sort_page->InsertTuple(t);
    }

    runs_.emplace(MergeSortRun{std::vector<page_id_t>{page_id}, bpm});

    entries.clear();
  };

  //* first 'run' that read data and sort them then write them back to disk
  while (child_executor_->Next(&child_tuple, &rid)) {
    entries.emplace_back(
        SortEntry{GenerateSortKey(child_tuple, plan_->GetOrderBy(), plan_->OutputSchema()), child_tuple});

    if (static_cast<int>(entries.size()) == max_size) {
      flush_to_disk();
    }
  }
  flush_to_disk();
}

template <size_t K>
void ExternalMergeSortExecutor<K>::HandleMergingTuples(int max_size, BufferPoolManager *bpm) {
  while (runs_.size() > 1) {
    auto page_runs_size = runs_.size();

    //* handle merging two runs
    while (page_runs_size >= 2) {
      page_runs_size -= 2;

      auto first_run = std::move(runs_.front());
      auto first_run_start_iter = first_run.Begin();
      auto first_run_end_iter = first_run.End();
      runs_.pop();

      auto second_run = std::move(runs_.front());
      auto second_run_start_iter = second_run.Begin();
      auto second_run_end_iter = second_run.End();
      runs_.pop();

      auto page_id = bpm->NewPage();
      auto page_guard = bpm->WritePage(page_id);
      auto sort_page = page_guard.AsMut<SortPage>();
      sort_page->Init(&plan_->OutputSchema());

      //* handle 2-way runs
      std::vector<page_id_t> page_ids;
      while (first_run_start_iter != first_run_end_iter || second_run_start_iter != second_run_end_iter) {
        if (first_run_start_iter != first_run_end_iter && second_run_start_iter != second_run_end_iter) {
          auto first_tuple = *first_run_start_iter;
          auto second_tuple = *second_run_start_iter;

          SortEntry first_run_sort_entry{GenerateSortKey(first_tuple, plan_->GetOrderBy(), plan_->OutputSchema()),
                                         Tuple{}};
          SortEntry second_run_sort_entry{GenerateSortKey(second_tuple, plan_->GetOrderBy(), plan_->OutputSchema()),
                                          Tuple{}};

          if (cmp_(first_run_sort_entry, second_run_sort_entry)) {
            sort_page->InsertTuple(first_tuple);
            ++first_run_start_iter;
          } else {
            sort_page->InsertTuple(second_tuple);
            ++second_run_start_iter;
          }
        } else if (first_run_start_iter != first_run_end_iter) {
          sort_page->InsertTuple(*first_run_start_iter);
          ++first_run_start_iter;

        } else if (second_run_start_iter != second_run_end_iter) {
          sort_page->InsertTuple(*second_run_start_iter);
          ++second_run_start_iter;
        }

        if (static_cast<int>(sort_page->GetSize()) == max_size) {
          page_ids.emplace_back(page_id);

          //* drop the current page since it's full and create new one
          page_guard.Drop();
          page_id = bpm->NewPage();
          page_guard = bpm->WritePage(page_id);
          sort_page = page_guard.AsMut<SortPage>();
          sort_page->Init(&plan_->OutputSchema());
        }
      }

      //* handle reamining entries and add their page id
      if (sort_page->GetSize() > 0) {
        page_ids.emplace_back(page_id);

        page_guard.Drop();
      } else {
        //* the page is empty so i have to delete it
        page_guard.Drop();
        bpm->DeletePage(page_id);
      }

      runs_.emplace(std::move(page_ids), bpm);
    }

    //* handle reamining only one run at the end
    if (page_runs_size == 1) {
      auto run = std::move(runs_.front());
      runs_.push(std::move(run));
      runs_.pop();
    }
  }
}

template <size_t K>
void ExternalMergeSortExecutor<K>::Init() {
  child_executor_->Init();

  int entry_size = static_cast<int>(sizeof(int32_t) + plan_->OutputSchema().GetInlinedStorageSize());
  int max_size = (BUSTUB_PAGE_SIZE - SORT_PAGE_HEADER_SIZE) / entry_size;
  auto bpm = exec_ctx_->GetBufferPoolManager();

  HandleSortingTuples(max_size, bpm);
  HandleMergingTuples(max_size, bpm);

  if (runs_.empty()) {
    //* if table is empty this handles it
    MergeSortRun run{std::vector<page_id_t>{}, bpm};

    start_iter_ = run.Begin();
    end_iter_ = run.End();
  } else {
    BUSTUB_ASSERT(runs_.size() == 1, "Runs have More Than one Run");

    final_run_ = std::move(runs_.front());
    runs_.pop();

    start_iter_ = final_run_.Begin();
    end_iter_ = final_run_.End();
  }
}

template <size_t K>
auto ExternalMergeSortExecutor<K>::Next(Tuple *tuple, RID *rid) -> bool {
  while (start_iter_ != end_iter_) {
    *tuple = *start_iter_;

    ++start_iter_;

    return true;
  }

  return false;
}

template <size_t K>
ExternalMergeSortExecutor<K>::~ExternalMergeSortExecutor() {
  //* if there still pages in the disk that didn't remove yet
  //* mainly this will happen because of LIMIT

  if (start_iter_ != end_iter_) {
    start_iter_.page_guard_.Drop();

    int i = start_iter_.page_index_;
    auto bpm = exec_ctx_->GetBufferPoolManager();

    for (; i < static_cast<int>(final_run_.GetPageCount()); i++) {
      bpm->DeletePage(final_run_.pages_[i]);
    }
  }
}

template class ExternalMergeSortExecutor<2>;

}  // namespace bustub
