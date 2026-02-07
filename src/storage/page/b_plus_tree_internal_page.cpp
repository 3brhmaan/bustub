//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_internal_page.cpp
//
// Copyright (c) 2018-2024, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <sstream>

#include "common/exception.h"
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_page.h"

namespace bustub {
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/
/*
 * Init method after creating a new internal page
 * Including set page type, set current size, and set max page size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(int max_size) {
  this->SetSize(1);  //* size is 1 since key[0] is invalid
  this->SetPageType(IndexPageType::INTERNAL_PAGE);
  this->SetMaxSize(max_size);
}
/*
 * Helper method to get/set the key associated with input "index" (a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const -> KeyType { return key_array_[index]; }

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) { key_array_[index] = key; }

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueIndex(const ValueType &value) const -> int {
  for (int i = 0; i < GetMaxSize(); i++) {
    if (page_id_array_[i] == value) {
      return i;
    }
  }

  return -1;
}

/*
 * Helper method to get the value associated with input "index" (a.k.a array
 * offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const -> ValueType { return page_id_array_[index]; }

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetValueAt(int index, const ValueType &value) { page_id_array_[index] = value; }

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Insert(const KeyType &key, const ValueType &value,
                                            const KeyComparator &comparator) {
  auto position = FindKeyIndex(key, comparator);

  //* Shift elements to the right to make space for the new key-value pair
  for (int i = GetSize() + 1; i > position; i--) {
    key_array_[i] = key_array_[i - 1];
    page_id_array_[i] = page_id_array_[i - 1];
  }

  //* Insert the new key-value pair
  key_array_[position] = key;
  page_id_array_[position] = value;

  //* Increment the size
  ChangeSizeBy(1);
}

/*
 * delete the key/value entry from the internal node
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Delete(int entry_index) {
  //* delete the key/value pair entry
  for (int i = entry_index; i < GetSize() - 1; i++) {
    key_array_[i] = key_array_[i + 1];
    page_id_array_[i] = page_id_array_[i + 1];
  }

  //* decrease the size
  ChangeSizeBy(-1);
}

/*
 * search for the index of a key > my key using binary search
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::FindKeyIndex(const KeyType &key, const KeyComparator &comparator) const -> int {
  int low = 1;  // since key[0] is invalid and start from key[1]
  int high = GetSize();

  while (low < high) {
    int mid = low + (high - low) / 2;
    if (comparator(KeyAt(mid), key) <= 0) {
      //* KeyAt(mid) <= key → too small, go right
      low = mid + 1;
    } else {
      //* KeyAt(mid) > key → candidate, but keep looking left
      high = mid;
    }
  }

  return low;
}

/*
 * helper method that help traversing the tree
 * used to get the pointer to the largest element <= my key
 * return the page id
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::Lookup(const KeyType &key, const KeyComparator &comparator) const -> ValueType {
  auto position = FindKeyIndex(key, comparator);

  return ValueAt(position - 1);
}

// valuetype for internalNode should be page id_t
template class BPlusTreeInternalPage<GenericKey<4>, page_id_t, GenericComparator<4>>;
template class BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>;
template class BPlusTreeInternalPage<GenericKey<16>, page_id_t, GenericComparator<16>>;
template class BPlusTreeInternalPage<GenericKey<32>, page_id_t, GenericComparator<32>>;
template class BPlusTreeInternalPage<GenericKey<64>, page_id_t, GenericComparator<64>>;
}  // namespace bustub
