//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/include/index/index_iterator.h
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
/**
 * index_iterator.h
 * For range scan of b+ tree
 */
#pragma once
#include <optional>
#include <utility>
#include "storage/page/b_plus_tree_leaf_page.h"
#include "storage/page/page_guard.h"

namespace bustub {

#define INDEXITERATOR_TYPE IndexIterator<KeyType, ValueType, KeyComparator>

INDEX_TEMPLATE_ARGUMENTS
class IndexIterator {
 public:
  // you may define your own constructor based on your member variables
  IndexIterator(ReadPageGuard guard, BufferPoolManager *bpm);
  IndexIterator();
  ~IndexIterator();  // NOLINT

  auto IsEnd() -> bool;

  auto operator*() -> std::pair<const KeyType &, const ValueType &>;

  auto operator++() -> IndexIterator &;

  auto operator==(const IndexIterator &itr) const -> bool {
    //* both are end iterator
    if (!guard_.has_value() && !itr.guard_.has_value()) {
      return true;
    }

    //* One is end, the other is not
    if (!guard_.has_value() || !itr.guard_.has_value()) {
      return false;
    }

    //* both aren't end iterator
    return (index_ == itr.index_) && (guard_->GetPageId() == itr.guard_->GetPageId());
  }

  auto operator!=(const IndexIterator &itr) const -> bool { return !(*this == itr); }

 private:
  // add your own private member variables here
  std::optional<ReadPageGuard> guard_;
  BufferPoolManager *bpm_;
  int index_;
};

}  // namespace bustub
