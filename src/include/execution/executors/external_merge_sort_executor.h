//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// external_merge_sort_executor.h
//
// Identification: src/include/execution/executors/external_merge_sort_executor.h
//
// Copyright (c) 2015-2024, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "buffer/buffer_pool_manager.h"
#include "catalog/schema.h"
#include "common/config.h"
#include "common/logger.h"
#include "common/macros.h"
#include "execution/execution_common.h"
#include "execution/executors/abstract_executor.h"
#include "execution/plans/sort_plan.h"
#include "storage/page/page_guard.h"
#include "storage/table/tuple.h"

namespace bustub {

#define SORT_PAGE_HEADER_SIZE 12

/**
 * Page to hold the intermediate data for external merge sort.

 |--- 12 bytes (Header) ---|------- 4084 bytes (data_) --------|
 | size_ | max_ | entry_   | [0][1][2][3] ... [4083]           |

 *
 * Only fixed-length data will be supported in Fall 2024.
 */
class SortPage {
 public:
  SortPage() = delete;
  SortPage(const SortPage &other) = delete;

  void Init(const Schema *schema) {
    size_ = 0;
    entry_size_ = static_cast<int>(sizeof(int32_t) + schema->GetInlinedStorageSize());
    max_size_ = (BUSTUB_PAGE_SIZE - SORT_PAGE_HEADER_SIZE) / entry_size_;
  }

  auto GetSize() const -> int { return size_; }
  auto GetMaxSize() const -> int { return max_size_; }
  auto GetEntrySize() const -> int { return entry_size_; }

  void SetSize(int size) { size_ = size; }
  void ChangeSizeBy(int amount) { size_ += amount; }

  void InsertTuple(const Tuple &tuple) {
    BUSTUB_ASSERT(size_ < max_size_, "the current size of tuples >= max size");

    char *dest = tuples_ + (size_ * entry_size_);

    tuple.SerializeTo(dest);
    ChangeSizeBy(1);
  }

  auto GetTupleAt(int index) const -> Tuple {
    BUSTUB_ASSERT(index >= 0 && index < size_, fmt::format("Bad index {} for page with size {}", index, size_).c_str());

    const char *src = tuples_ + (index * entry_size_);

    Tuple tuple;
    tuple.DeserializeFrom(src);
    return tuple;
  }

  auto ToString(const Schema *schema) const -> std::string {
    std::string result = "Tuples From SortPage: \n";

    for (int i = 0; i < GetSize(); i++) {
      auto tuple = GetTupleAt(i);

      result += tuple.ToString(schema) + "\n";
    }

    return result;
  }

 private:
  /*represent the current size of the array of data*/
  int size_;
  /*represent the max size that the array can have of data*/
  int max_size_;
  /*represent each entry size*/
  int entry_size_;
  /*array of Tuples*/
  char tuples_[BUSTUB_PAGE_SIZE - SORT_PAGE_HEADER_SIZE];
};

/**
 * A data structure that holds the sorted tuples as a run during external merge sort.
 * Tuples might be stored in multiple pages, and tuples are ordered both within one page
 * and across pages.
 */
class MergeSortRun {
 public:
  MergeSortRun() = default;
  MergeSortRun(std::vector<page_id_t> pages, BufferPoolManager *bpm) : pages_(std::move(pages)), bpm_(bpm) {}

  //* delete copy constructor and operator
  MergeSortRun(const MergeSortRun &) = delete;
  auto operator=(const MergeSortRun &) -> MergeSortRun & = delete;

  MergeSortRun(MergeSortRun &&) = default;
  auto operator=(MergeSortRun &&) -> MergeSortRun & = default;

  auto GetPageCount() -> size_t { return pages_.size(); }
  auto GetPageCount() const -> size_t { return pages_.size(); }

  /** Iterator for iterating on the sorted tuples in one run. */
  class Iterator {
    friend class MergeSortRun;
    template <size_t K>
    friend class ExternalMergeSortExecutor;

   public:
    Iterator() = default;

    /**
     * Advance the iterator to the next tuple. If the current sort page is exhausted, move to the
     * next sort page.
     */
    auto operator++() -> Iterator & {
      tuple_index_++;

      if (tuple_index_ >= sort_page_->GetSize()) {
        page_guard_.Drop();
        run_->bpm_->DeletePage(run_->pages_[page_index_]);

        page_index_++;
        tuple_index_ = 0;

        if (page_index_ < static_cast<int>(run_->GetPageCount())) {
          LoadCurrentPage();
        }
      }

      if (page_index_ >= static_cast<int>(run_->GetPageCount())) {
        page_index_ = -1;
        sort_page_ = nullptr;
      }

      return *this;
    }

    /**
     * Dereference the iterator to get the current tuple in the sorted run that the iterator is
     * pointing to.
     */
    auto operator*() -> Tuple { return sort_page_->GetTupleAt(tuple_index_); }

    /**
     * Checks whether two iterators are pointing to the same tuple in the same sorted run.
     */
    auto operator==(const Iterator &other) const -> bool {
      return this->page_index_ == other.page_index_ && this->tuple_index_ == other.tuple_index_;
    }

    /**
     * Checks whether two iterators are pointing to different tuples in a sorted run or iterating
     * on different sorted runs.
     */
    auto operator!=(const Iterator &other) const -> bool { return !(*this == other); }

    void LoadCurrentPage() {
      int page_id = run_->pages_[page_index_];
      page_guard_ = run_->bpm_->ReadPage(page_id);
      sort_page_ = page_guard_.As<SortPage>();
    }

   private:
    explicit Iterator(const MergeSortRun *run, int page_index = 0) : run_(run), page_index_(page_index) {
      if (page_index != -1 && page_index < static_cast<int>(run_->GetPageCount())) {
        LoadCurrentPage();
      }
    }

    /** The sorted run that the iterator is iterating on. */
    [[maybe_unused]] const MergeSortRun *run_;

    ReadPageGuard page_guard_{};
    const SortPage *sort_page_{nullptr};
    int page_index_{0};
    int tuple_index_{0};
  };

  /**
   * Get an iterator pointing to the beginning of the sorted run, i.e. the first tuple.
   */
  auto Begin() -> Iterator {
    if (pages_.empty()) {
      return End();
    }

    return Iterator{this};
  }

  /**
   * Get an iterator pointing to the end of the sorted run, i.e. the position after the last tuple.
   */
  auto End() -> Iterator { return Iterator{this, -1}; }

  //  private:
  /** The page IDs of the sort pages that store the sorted tuples. */
  std::vector<page_id_t> pages_;
  /**
   * The buffer pool manager used to read sort pages. The buffer pool manager is responsible for
   * deleting the sort pages when they are no longer needed.
   */
  [[maybe_unused]] BufferPoolManager *bpm_;
};

/**
 * ExternalMergeSortExecutor executes an external merge sort.
 *
 * In Fall 2024, only 2-way external merge sort is required.
 */
template <size_t K>
class ExternalMergeSortExecutor : public AbstractExecutor {
 public:
  ExternalMergeSortExecutor(ExecutorContext *exec_ctx, const SortPlanNode *plan,
                            std::unique_ptr<AbstractExecutor> &&child_executor);

  ~ExternalMergeSortExecutor() override;

  /** Initialize the external merge sort */
  void Init() override;

  /**
   * Yield the next tuple from the external merge sort.
   * @param[out] tuple The next tuple produced by the external merge sort.
   * @param[out] rid The next tuple RID produced by the external merge sort.
   * @return `true` if a tuple was produced, `false` if there are no more tuples
   */
  auto Next(Tuple *tuple, RID *rid) -> bool override;

  /** @return The output schema for the external merge sort */
  auto GetOutputSchema() const -> const Schema & override { return plan_->OutputSchema(); }

 private:
  void HandleSortingTuples(int max_size, BufferPoolManager *bpm);
  void HandleMergingTuples(int max_size, BufferPoolManager *bpm);

  /** The sort plan node to be executed */
  const SortPlanNode *plan_;

  /** Compares tuples based on the order-bys */
  TupleComparator cmp_;

  /** TODO: You will want to add your own private members here. */
  std::unique_ptr<AbstractExecutor> child_executor_;
  std::queue<MergeSortRun> runs_;
  MergeSortRun final_run_;
  MergeSortRun::Iterator start_iter_;
  MergeSortRun::Iterator end_iter_;
};

}  // namespace bustub
