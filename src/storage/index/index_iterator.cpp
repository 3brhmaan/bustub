/**
 * index_iterator.cpp
 */
#include <cassert>
#include <optional>
#include <utility>
#include "common/config.h"
#include "common/macros.h"
#include "storage/page/b_plus_tree_leaf_page.h"
#include "storage/page/b_plus_tree_page.h"
#include "storage/page/page_guard.h"

#include "storage/index/index_iterator.h"

namespace bustub {

/*
 * NOTE: you can change the destructor/constructor method here
 * set your own input parameters
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator(ReadPageGuard guard, BufferPoolManager *bpm)
    : guard_(std::move(guard)), bpm_(bpm), index_(0) {}
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator() : guard_(std::nullopt) {}
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() {
  if (guard_.has_value()) {
    guard_->Drop();
  }
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool {
  // auto current_node = guard_->As<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>>();

  // return index_ >= current_node->GetSize();

  return !guard_.has_value();
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator*() -> std::pair<const KeyType &, const ValueType &> {
  BUSTUB_ASSERT(!IsEnd(), "Cannot Get Key/Value From end iterator");

  auto current_node = guard_->As<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>>();

  return std::make_pair(current_node->KeyAt(index_), current_node->ValueAt(index_));
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
  BUSTUB_ASSERT(!IsEnd(), "Cannot increment end iterator");

  auto current_node = guard_->As<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>>();
  index_++;

  if (index_ >= current_node->GetSize()) {
    auto next_page_id = current_node->GetNextPageId();
    index_ = 0;

    if (next_page_id == INVALID_PAGE_ID) {
      guard_ = std::nullopt;
    } else {
      guard_ = bpm_->ReadPage(next_page_id);
    }
  }

  return *this;
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;

template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;

template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;

template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;

template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
