//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_leaf_page.cpp
//
// Copyright (c) 2018-2024, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <sstream>

#include "common/config.h"
#include "common/exception.h"
#include "common/logger.h"
#include "common/macros.h"
#include "common/rid.h"
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {

/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/

/**
 * Init method after creating a new leaf page
 * Including set page type, set current size to zero, set next page id and set max size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(int max_size) {
  this->SetSize(0);
  this->SetPageType(IndexPageType::LEAF_PAGE);
  this->SetMaxSize(max_size);

  next_page_id_ = INVALID_PAGE_ID;
}

/**
 * Helper methods to set/get next page id
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetNextPageId() const -> page_id_t { return next_page_id_; }

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::SetNextPageId(page_id_t next_page_id) { next_page_id_ = next_page_id; }

/*
 * Helper method to find and return the key associated with input "index" (a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const -> KeyType { return key_array_[index]; }

/*
 * search for the index of a key index >= my key using binary search
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::FindKeyIndex(const KeyType &key, const KeyComparator &comparator) const -> int {
  int left = 0;
  int right = GetSize();

  while (left < right) {
    int mid = left + (right - left) / 2;
    if (comparator(key_array_[mid], key) < 0) {
      //* key_array_[mid] < MyKey
      left = mid + 1;
    } else {
      //* key_array_[mid] >= MyKey
      right = mid;
    }
  }

  return left;
}

/*
 * insert the key/value in the leaf node
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::Insert(const KeyType &key, const ValueType &value, const KeyComparator &comparator) {
  int insert_pos = FindKeyIndex(key, comparator);

  //* Shift elements to the right to make space for the new key-value pair
  for (int i = GetSize(); i > insert_pos; i--) {
    key_array_[i] = key_array_[i - 1];
    rid_array_[i] = rid_array_[i - 1];
  }

  //* Insert the new key-value pair
  key_array_[insert_pos] = key;
  rid_array_[insert_pos] = value;

  //* Increment the size
  ChangeSizeBy(1);
}

/*
 * delete key/value from the leaf and do nothing if doesn't exist
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::Delete(const KeyType &key, const KeyComparator &comparator) {
  auto delete_position = FindKeyIndex(key, comparator);

  //* delete the key/value pair
  for (int i = delete_position; i < GetSize() - 1; i++) {
    key_array_[i] = key_array_[i + 1];
    rid_array_[i] = rid_array_[i + 1];
  }

  //* decrease the size
  ChangeSizeBy(-1);
}

template class BPlusTreeLeafPage<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTreeLeafPage<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTreeLeafPage<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTreeLeafPage<GenericKey<64>, RID, GenericComparator<64>>;
}  // namespace bustub
