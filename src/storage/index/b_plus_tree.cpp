#include "storage/index/b_plus_tree.h"
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include "common/config.h"
#include "common/exception.h"
#include "common/logger.h"
#include "common/macros.h"
#include "storage/index/b_plus_tree_debug.h"
#include "storage/index/index_iterator.h"
#include "storage/page/b_plus_tree_header_page.h"
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_leaf_page.h"
#include "storage/page/b_plus_tree_page.h"
#include "storage/page/page_guard.h"

namespace bustub {

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                          const KeyComparator &comparator, int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      bpm_(buffer_pool_manager),
      comparator_(std::move(comparator)),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size),
      header_page_id_(header_page_id) {
  WritePageGuard guard = bpm_->WritePage(header_page_id_);
  auto root_page = guard.AsMut<BPlusTreeHeaderPage>();
  root_page->root_page_id_ = INVALID_PAGE_ID;
}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool {
  auto guard = bpm_->ReadPage(header_page_id_);
  auto root_page = guard.As<BPlusTreeHeaderPage>();

  return root_page->root_page_id_ == INVALID_PAGE_ID;
}

/*****************************************************************************
 * TRAVERSING
 *****************************************************************************/

/*
 * traverse the tree from the root until reach the leaf using a KEY
 * return the leaf read guard
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::TraverseUntilLeaf(const KeyType &key, Context &ctx) -> ReadPageGuard {
  //* read the root page
  auto guard = bpm_->ReadPage(ctx.root_page_id_);

  //* convert it to base class node to check it's type until reaching the leaf pages
  auto node = guard.template As<BPlusTreePage>();

  //* release the lock from parent which is header_gurad in this case
  ctx.read_header_page_ = std::nullopt;

  while (!node->IsLeafPage()) {
    //* the node is an internal node so convert to it and keep looping until finding the leaf page
    auto internal_node = guard.template As<InternalPage>();

    auto next_node_id = internal_node->Lookup(key, comparator_);

    //* acquire latch on child and then release the latch of the parent
    auto child_guard = bpm_->ReadPage(next_node_id);
    guard.Drop();

    guard = std::move(child_guard);
    node = guard.template As<BPlusTreePage>();
  }

  return guard;
}

/*
 * traverse the tree from the root until reach the left most leaf using a KEY
 * return the leaf read guard
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::TraverseUntilLeftMostLeaf() -> ReadPageGuard {
  //* read the root page
  auto root_page_id = GetRootPageId();
  auto guard = bpm_->ReadPage(root_page_id);

  //* convert it to base class node to check it's type until reaching the leaf pages
  auto node = guard.template As<BPlusTreePage>();

  while (!node->IsLeafPage()) {
    //* the node is an internal node so convert to it and keep looping until finding the leaf page
    auto internal_node = guard.template As<InternalPage>();

    auto next_node_id = internal_node->ValueAt(0);

    //* acquire latch on child and then release the latch of the parent
    auto child_guard = bpm_->ReadPage(next_node_id);
    guard.Drop();

    guard = std::move(child_guard);
    node = guard.template As<BPlusTreePage>();
  }

  return guard;
}

/*
 * traverse the tree from the root until reach the leaf using a KEY and keeping the write guard and the header page
 * guard and root id used for insert/delete operation to track the guards 'context' store the write guards from the root
 * till the leaf and root page id and header write guard
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::TraverseTree(const KeyType &key, Context &ctx, OperationType type) {
  //* read the root page
  auto header_page = ctx.header_page_.value().As<BPlusTreeHeaderPage>();
  auto guard = bpm_->WritePage(header_page->root_page_id_);

  //* store the root page id
  ctx.root_page_id_ = header_page->root_page_id_;

  auto node = guard.template As<BPlusTreePage>();

  while (!node->IsLeafPage()) {
    //* the node is an internal node so convert to it and keep looping until finding the leaf page
    auto internal_node = guard.template As<InternalPage>();

    auto next_node_id = internal_node->Lookup(key, comparator_);

    //* save the current guard in context
    ctx.write_set_.push_back(std::move(guard));

    //* acquire lock on child guard
    auto child_guard = bpm_->WritePage(next_node_id);
    auto child_node = child_guard.template As<BPlusTreePage>();

    //* check if it's safe to release locks of ancestors depends on the operation type + header guard
    if (type == OperationType::INSERT) {
      //* a safe node that will not split on insert (check type first, since leaf have max size = max_size - 1)

      if ((child_node->IsLeafPage() && child_node->GetSize() + 1 < child_node->GetMaxSize()) ||
          (!child_node->IsLeafPage() && child_node->GetSize() < child_node->GetMaxSize())) {
        //* release all locks + header page lock
        ctx.header_page_ = std::nullopt;
        while (!ctx.write_set_.empty()) {
          ctx.write_set_.pop_front();
        }
      }

    } else {
      //* a safe node that have more than half full during deletion

      if (child_node->GetSize() > child_node->GetMinSize()) {
        //* release all locks + header page lock
        ctx.header_page_ = std::nullopt;
        while (!ctx.write_set_.empty()) {
          ctx.write_set_.pop_front();
        }
      }
    }

    guard = std::move(child_guard);
    node = child_node;
  }

  //* store the leaf guard
  ctx.write_set_.push_back(std::move(guard));
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result) -> bool {
  //* set root it and header
  Context ctx;
  ctx.read_header_page_ = bpm_->ReadPage(header_page_id_);
  auto header_node = ctx.read_header_page_.value().As<BPlusTreeHeaderPage>();
  ctx.root_page_id_ = header_node->root_page_id_;

  //* if tree is empty return false
  if (header_node->root_page_id_ == INVALID_PAGE_ID) {
    return false;
  }

  auto guard = TraverseUntilLeaf(key, ctx);
  auto leaf_node = guard.template As<LeafPage>();

  //* search the leaf node for a key >= my key
  auto position = leaf_node->FindKeyIndex(key, comparator_);

  if (position < leaf_node->GetSize() && comparator_(leaf_node->KeyAt(position), key) == 0) {
    result->push_back(leaf_node->ValueAt(position));
    return true;
  }

  return false;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value) -> bool {
  Context ctx;

  //* get the header gaurd and save it in ctx
  ctx.header_page_ = bpm_->WritePage(header_page_id_);
  auto header_page = ctx.header_page_.value().AsMut<BPlusTreeHeaderPage>();

  //* if tree is empty, create a new root and update the header page id
  if (header_page->root_page_id_ == INVALID_PAGE_ID) {
    //* create new node to act as the root and initialize it
    auto page_id = bpm_->NewPage();
    auto guard = bpm_->WritePage(page_id);
    auto leaf_node = guard.AsMut<LeafPage>();
    leaf_node->Init(leaf_max_size_);

    //* insert the key/value in the leaf node
    leaf_node->Insert(key, value, comparator_);

    //* update the root page id and drop its guard
    header_page->root_page_id_ = page_id;

    return true;
  }

  TraverseTree(key, ctx, OperationType::INSERT);

  //* get the leaf write guard
  auto &write_guard = ctx.write_set_.back();
  auto leaf_node = write_guard.template AsMut<LeafPage>();

  //* check if duplicates keys
  int insert_pos = leaf_node->FindKeyIndex(key, comparator_);
  if (insert_pos < leaf_node->GetSize() && comparator_(leaf_node->KeyAt(insert_pos), key) == 0) {
    return false;
  }

  leaf_node->Insert(key, value, comparator_);

  //* split a leaf node when the number of values reaches max_size after insertion
  if (leaf_node->GetSize() == leaf_node->GetMaxSize()) {
    HandlePageSplit(ctx);
  }

  return true;
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::HandlePageSplit(Context &ctx) {
  //* get the old leaf node - act as left leaf -
  auto &left_leaf_guard = ctx.write_set_.back();
  auto left_leaf_node = left_leaf_guard.AsMut<LeafPage>();

  //* create new leaf - act as right leaf - node and initialize it
  auto right_leaf_page_id = bpm_->NewPage();
  auto right_leaf_guard = bpm_->WritePage(right_leaf_page_id);
  auto right_leaf_node = right_leaf_guard.AsMut<LeafPage>();
  right_leaf_node->Init(leaf_max_size_);

  //* insert ceil(n/2) keys/values to the new right leaf node
  for (int i = left_leaf_node->GetMinSize(); i < left_leaf_node->GetSize(); i++) {
    right_leaf_node->Insert(left_leaf_node->KeyAt(i), left_leaf_node->ValueAt(i), comparator_);
  }

  //* change the size of the old leaf node
  left_leaf_node->ChangeSizeBy(left_leaf_node->GetMinSize() - left_leaf_node->GetMaxSize());

  //* handle sibling pointers
  right_leaf_node->SetNextPageId(left_leaf_node->GetNextPageId());
  left_leaf_node->SetNextPageId(right_leaf_page_id);

  //? -----------------------Manage Split into 2 leaf nodes-----------------------------------

  if (ctx.IsRootPage(left_leaf_guard.GetPageId())) {
    //* create an internal node and initilize it to act as the new root node
    auto new_internal_page_id = bpm_->NewPage();
    auto new_internal_guard = bpm_->WritePage(new_internal_page_id);
    auto new_internal_node = new_internal_guard.AsMut<InternalPage>();
    new_internal_node->Init(internal_max_size_);

    //* copy the fisrt key/page_id from the right leaf and insert in the parent node
    new_internal_node->Insert(right_leaf_node->KeyAt(0), right_leaf_page_id, comparator_);

    //* update the root id
    auto header_page = ctx.header_page_.value().AsMut<BPlusTreeHeaderPage>();
    ctx.root_page_id_ = new_internal_page_id;
    header_page->root_page_id_ = new_internal_page_id;

    //* make value[0] point to the left node
    new_internal_node->SetValueAt(0, left_leaf_guard.GetPageId());

    left_leaf_guard.Drop();
    right_leaf_guard.Drop();
  } else {
    //* get the parent of the current leaf and insert the key/page_id in it
    auto current_guard_index = ctx.write_set_.size() - 2;
    auto &parent_guard = ctx.write_set_[current_guard_index];
    auto parent_internal_node = parent_guard.AsMut<InternalPage>();

    auto right_leaf_page_first_key = right_leaf_node->KeyAt(0);

    if (parent_internal_node->GetSize() == parent_internal_node->GetMaxSize()) {
      HandleInternalPageSplit(ctx, right_leaf_page_first_key, right_leaf_page_id, current_guard_index);
    } else {
      parent_internal_node->Insert(right_leaf_page_first_key, right_leaf_page_id, comparator_);
    }

    right_leaf_guard.Drop();
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::HandleInternalPageSplit(Context &ctx, const KeyType &key, page_id_t value,
                                             size_t current_guard_index) {
  auto &current_guard = ctx.write_set_[current_guard_index];
  auto current_node = current_guard.AsMut<InternalPage>();

  if (current_node->GetSize() == current_node->GetMaxSize()) {
    //* split the node and insert in the new created/right node ceil(n/2) keys
    auto new_node_id = bpm_->NewPage();
    auto new_guard = bpm_->WritePage(new_node_id);
    auto new_node = new_guard.AsMut<InternalPage>();
    new_node->Init(internal_max_size_);

    for (int i = current_node->GetMinSize(); i < current_node->GetSize(); i++) {
      new_node->Insert(current_node->KeyAt(i), current_node->ValueAt(i), comparator_);
    }

    current_node->SetSize(current_node->GetMinSize());

    //* insert the key/value in the correct node
    if (comparator_(key, new_node->KeyAt(1)) < 0) {
      //* key < new_node_keys[1]
      current_node->Insert(key, value, comparator_);

      auto current_node_last_key = current_node->KeyAt(current_node->GetSize() - 1);
      auto current_node_last_value = current_node->ValueAt(current_node->GetSize() - 1);

      current_node->ChangeSizeBy(-1);

      new_node->Insert(current_node_last_key, current_node_last_value, comparator_);
    } else {
      //* key >= new_node_keys[1]
      new_node->Insert(key, value, comparator_);
    }

    //* copy value[1] to value[0] in the new create/right node CUZ key[1] will be promoted to the parent
    new_node->SetValueAt(0, new_node->ValueAt(1));

    //* if it's not root then move first key of the new create/right node to the parent and the value will be the
    if (ctx.IsRootPage(current_guard.GetPageId())) {
      //* create a root node and initilize it
      auto root_page_id = bpm_->NewPage();
      auto root_guard = bpm_->WritePage(root_page_id);
      auto root_node = root_guard.AsMut<InternalPage>();
      root_node->Init(internal_max_size_);

      //* set root key[1]&value[1] to be the new created node key[1]&value[1] and value[0] the old node
      root_node->Insert(new_node->KeyAt(1), new_node_id, comparator_);
      root_node->SetValueAt(0, current_guard.GetPageId());

      //* shift the keys/values left start from index 1 and change the size by 1
      for (int i = 1; i < new_node->GetSize() - 1; i++) {
        new_node->SetKeyAt(i, new_node->KeyAt(i + 1));
        new_node->SetValueAt(i, new_node->ValueAt(i + 1));
      }
      new_node->ChangeSizeBy(-1);

      //* update the root id
      auto header_page = ctx.header_page_.value().AsMut<BPlusTreeHeaderPage>();
      ctx.root_page_id_ = root_page_id;
      header_page->root_page_id_ = root_page_id;

      //* drop
      new_guard.Drop();
      root_guard.Drop();
    } else {
      //* get the key/value that will be inserted in the parent
      auto parent_key = new_node->KeyAt(1);
      auto parent_value = new_node_id;

      //* shift the keys/values left start from index 1
      for (int i = 1; i < new_node->GetSize() - 1; i++) {
        new_node->SetKeyAt(i, new_node->KeyAt(i + 1));
        new_node->SetValueAt(i, new_node->ValueAt(i + 1));
      }

      new_node->ChangeSizeBy(-1);

      new_guard.Drop();

      HandleInternalPageSplit(ctx, parent_key, parent_value, current_guard_index - 1);
    }

  } else {
    current_node->Insert(key, value, comparator_);
  }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/

/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key) {
  Context ctx;

  //* if tree is empty return
  ctx.header_page_ = bpm_->WritePage(header_page_id_);
  auto header_page = ctx.header_page_.value().As<BPlusTreeHeaderPage>();

  if (header_page->root_page_id_ == INVALID_PAGE_ID) {
    return;
  }

  TraverseTree(key, ctx, OperationType::DELETE);

  auto &leaf_guard = ctx.write_set_.back();
  auto leaf_node = leaf_guard.AsMut<LeafPage>();

  //* check if key exist in leaf node
  auto delete_position = leaf_node->FindKeyIndex(key, comparator_);

  //* do nothing if the key doesn't exist
  if (delete_position >= leaf_node->GetSize() || comparator_(leaf_node->KeyAt(delete_position), key) != 0) {
    return;
  }

  leaf_node->Delete(key, comparator_);

  //* if after delete operation the leaf size become under flow
  if (leaf_node->GetSize() < leaf_node->GetMinSize()) {
    HandlePageUnederFlow(ctx);
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::HandlePageUnederFlow(Context &ctx) {
  auto &current_leaf_guard = ctx.write_set_.back();
  auto current_leaf_node = current_leaf_guard.AsMut<LeafPage>();
  auto current_leaf_page_id = current_leaf_guard.GetPageId();

  //* check if the current leaf is the root and it's size is 0
  if (ctx.IsRootPage(current_leaf_page_id) && current_leaf_node->GetSize() == 0) {
    //* update the root id to be invalid
    auto header_page = ctx.header_page_->AsMut<BPlusTreeHeaderPage>();
    header_page->root_page_id_ = INVALID_PAGE_ID;
    ctx.root_page_id_ = INVALID_PAGE_ID;

    //* drop the page to delete it
    current_leaf_guard.Drop();

  } else if (!ctx.IsRootPage(current_leaf_page_id)) {
    auto &parent_guard = ctx.write_set_[ctx.write_set_.size() - 2];
    auto parent_internal_node = parent_guard.AsMut<InternalPage>();

    int current_leaf_value_index = parent_internal_node->ValueIndex(current_leaf_page_id);

    //* delete the leaf and it's entry from parent if is empty
    if (current_leaf_node->GetSize() == 0) {
      if (current_leaf_value_index > 0) {
        //* get the left sibling
        page_id_t left_leaf_sibling_id = parent_internal_node->ValueAt(current_leaf_value_index - 1);
        auto left_leaf_sibling_guard = bpm_->WritePage(left_leaf_sibling_id);
        auto left_leaf_sibling_node = left_leaf_sibling_guard.AsMut<LeafPage>();

        //* fix the sibling pointer
        left_leaf_sibling_node->SetNextPageId(current_leaf_node->GetNextPageId());

        //* delete my leaf and it's entry if i have left sibling
        parent_internal_node->Delete(current_leaf_value_index);

        //* drop the page to delete it
        left_leaf_sibling_guard.Drop();
        current_leaf_guard.Drop();
      } else {
        //* if i'm the left most child and empty, then move the right sigling content in me and remove the right entry
        //* and it's node from disk

        auto right_leaf_sibling_id = parent_internal_node->ValueAt(current_leaf_value_index + 1);
        auto right_leaf_sibling_guard = bpm_->WritePage(right_leaf_sibling_id);
        auto right_leaf_sibling_node = right_leaf_sibling_guard.template AsMut<LeafPage>();

        //* move the right data in my leaf
        for (int i = 0; i < right_leaf_sibling_node->GetSize(); i++) {
          current_leaf_node->Insert(right_leaf_sibling_node->KeyAt(i), right_leaf_sibling_node->ValueAt(i),
                                    comparator_);
        }

        //* fix sibling pointer
        current_leaf_node->SetNextPageId(right_leaf_sibling_node->GetNextPageId());

        //* remove the right sibling entry and it's page form disk
        parent_internal_node->Delete(current_leaf_value_index + 1);
        right_leaf_sibling_guard.Drop();
      }
    } else {
      //* get left and right sibling nodes if they exist
      page_id_t left_leaf_sibling_id = INVALID_PAGE_ID;
      page_id_t right_leaf_sibling_id = INVALID_PAGE_ID;

      if (current_leaf_value_index > 0) {
        left_leaf_sibling_id = parent_internal_node->ValueAt(current_leaf_value_index - 1);
      }

      if (current_leaf_value_index < parent_internal_node->GetSize() - 1) {
        right_leaf_sibling_id = parent_internal_node->ValueAt(current_leaf_value_index + 1);
      }

      //* try to borrow or merge with the right sibling else try same with left sibling
      if (right_leaf_sibling_id != INVALID_PAGE_ID) {
        auto right_leaf_sibling_guard = bpm_->WritePage(right_leaf_sibling_id);
        auto right_leaf_sibling_node = right_leaf_sibling_guard.AsMut<LeafPage>();

        //* try to borrow from right sibling if can't then merge with the right
        if (right_leaf_sibling_node->GetSize() > right_leaf_sibling_node->GetMinSize()) {
          //* insert the first key/value of the right sibling in the current leaf and delete from that sibling
          auto right_leaf_sibling_first_key = right_leaf_sibling_node->KeyAt(0);
          auto right_leaf_sibling_first_value = right_leaf_sibling_node->ValueAt(0);

          current_leaf_node->Insert(right_leaf_sibling_first_key, right_leaf_sibling_first_value, comparator_);
          right_leaf_sibling_node->Delete(right_leaf_sibling_first_key, comparator_);

          //* update parent's of the right sibling with the new key
          parent_internal_node->SetKeyAt(current_leaf_value_index + 1, right_leaf_sibling_node->KeyAt(0));

          right_leaf_sibling_guard.Drop();
        } else {
          //* insert all the key/value of the right sibling in my leaf
          for (int i = 0; i < right_leaf_sibling_node->GetSize(); i++) {
            current_leaf_node->Insert(right_leaf_sibling_node->KeyAt(i), right_leaf_sibling_node->ValueAt(i),
                                      comparator_);
          }

          //* delete the entry that point to the right sibling from parent
          parent_internal_node->Delete(current_leaf_value_index + 1);

          //* handle sibling pointer
          current_leaf_node->SetNextPageId(right_leaf_sibling_node->GetNextPageId());

          //* delete the right sibling page from disk
          right_leaf_sibling_guard.Drop();
        }
      } else {
        auto left_leaf_sibling_guard = bpm_->WritePage(left_leaf_sibling_id);
        auto left_leaf_sibling_node = left_leaf_sibling_guard.AsMut<LeafPage>();

        //* try to borrow from left sibling if can't then merge with the left
        if (left_leaf_sibling_node->GetSize() > left_leaf_sibling_node->GetMinSize()) {
          //* insert the last key/value of the left sibling in the current leaf and delete from that sibling
          auto left_leaf_sibling_last_key = left_leaf_sibling_node->KeyAt(left_leaf_sibling_node->GetSize() - 1);
          auto left_leaf_sibling_last_value = left_leaf_sibling_node->ValueAt(left_leaf_sibling_node->GetSize() - 1);

          current_leaf_node->Insert(left_leaf_sibling_last_key, left_leaf_sibling_last_value, comparator_);
          left_leaf_sibling_node->Delete(left_leaf_sibling_last_key, comparator_);

          //* update parent's of the current leaf with the new first key
          parent_internal_node->SetKeyAt(current_leaf_value_index, current_leaf_node->KeyAt(0));

          left_leaf_sibling_guard.Drop();
        } else {
          //* insert all the key/value of my leaf in the left sibling
          for (int i = 0; i < current_leaf_node->GetSize(); i++) {
            left_leaf_sibling_node->Insert(current_leaf_node->KeyAt(i), current_leaf_node->ValueAt(i), comparator_);
          }

          //* delete the current leaf entry from parent
          parent_internal_node->Delete(current_leaf_value_index);

          //* handle sibling pointer
          left_leaf_sibling_node->SetNextPageId(current_leaf_node->GetNextPageId());

          //* delete the current leaf page from disk
          left_leaf_sibling_guard.Drop();
          current_leaf_guard.Drop();
        }
      }
    }

    //* handle parent internal node if it's under flow
    if (parent_internal_node->GetSize() < parent_internal_node->GetMinSize()) {
      HandleInternalUnderflow(ctx, ctx.write_set_.size() - 2);
    }
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::HandleInternalUnderflow(Context &ctx, size_t guard_index) {
  auto &current_guard = ctx.write_set_[guard_index];
  auto current_node = current_guard.AsMut<InternalPage>();
  auto current_page_id = current_guard.GetPageId();

  //* if it's root and under full "size < 2", then delete this node and set it's child as the root
  if (ctx.IsRootPage(current_page_id)) {
    if (current_node->GetSize() < 2) {
      page_id_t new_root_page_id = current_node->ValueAt(0);

      //* update the root page id
      auto header_page = ctx.header_page_->AsMut<BPlusTreeHeaderPage>();
      header_page->root_page_id_ = new_root_page_id;
      ctx.root_page_id_ = new_root_page_id;

      //* delete the current root node
      current_guard.Drop();
    }

    return;
  }

  //* get left & right sibling nodes if exist
  auto &parent_guard = ctx.write_set_[guard_index - 1];
  auto parent_node = parent_guard.AsMut<InternalPage>();

  int current_internal_value_index = parent_node->ValueIndex(current_page_id);

  page_id_t left_sibling_id = INVALID_PAGE_ID;
  page_id_t right_sibling_id = INVALID_PAGE_ID;

  if (current_internal_value_index > 0) {
    left_sibling_id = parent_node->ValueAt(current_internal_value_index - 1);
  }

  if (current_internal_value_index < parent_node->GetSize() - 1) {
    right_sibling_id = parent_node->ValueAt(current_internal_value_index + 1);
  }

  //* try to borrow or merge with the right sibling else try same with left sibling
  if (right_sibling_id != INVALID_PAGE_ID) {
    auto right_sibling_guard = bpm_->WritePage(right_sibling_id);
    auto right_sibling_node = right_sibling_guard.AsMut<InternalPage>();

    //* if there is extra keys in right sibling then borrow, else merge
    if (right_sibling_node->GetSize() > right_sibling_node->GetMinSize()) {
      //* get the right separator and insert it with right sibling value[0] in my node
      auto right_separator_key = parent_node->KeyAt(current_internal_value_index + 1);
      current_node->Insert(right_separator_key, right_sibling_node->ValueAt(0), comparator_);

      //* update the right separator
      parent_node->SetKeyAt(current_internal_value_index + 1, right_sibling_node->KeyAt(1));

      //* remove value[0] and key[1] from right sibling
      for (int i = 0; i < right_sibling_node->GetSize() - 1; i++) {
        right_sibling_node->SetValueAt(i, right_sibling_node->ValueAt(i + 1));

        if (i > 0) {
          right_sibling_node->SetKeyAt(i, right_sibling_node->KeyAt(i + 1));
        }
      }

      right_sibling_node->ChangeSizeBy(-1);

      right_sibling_guard.Drop();

    } else {
      //* get the right separator and insert it with right sibling value[0] in my node
      auto right_separator_key = parent_node->KeyAt(current_internal_value_index + 1);
      current_node->Insert(right_separator_key, right_sibling_node->ValueAt(0), comparator_);

      //* insert rest of keys/values of the right sibling in my node
      for (int i = 1; i < right_sibling_node->GetSize(); i++) {
        current_node->Insert(right_sibling_node->KeyAt(i), right_sibling_node->ValueAt(i), comparator_);
      }

      //* delete the right entry from the parent and from disk
      parent_node->Delete(current_internal_value_index + 1);

      right_sibling_guard.Drop();
    }

  } else {
    auto left_sibling_guard = bpm_->WritePage(left_sibling_id);
    auto left_sibling_node = left_sibling_guard.AsMut<InternalPage>();

    //* if there is extra keys in left sibling then borrow, else merge
    if (left_sibling_node->GetSize() > left_sibling_node->GetMinSize()) {
      //* get the my separator and insert it with my value[0] in my node
      auto current_separator_key = parent_node->KeyAt(current_internal_value_index);
      current_node->Insert(current_separator_key, current_node->ValueAt(0), comparator_);

      //* update my separator with the last key in the left sibling
      parent_node->SetKeyAt(current_internal_value_index, left_sibling_node->KeyAt(left_sibling_node->GetSize() - 1));

      //* update my value [0] with the last value in the left sibling
      current_node->SetValueAt(0, left_sibling_node->ValueAt(left_sibling_node->GetSize() - 1));

      //* remove the last/key value from left sibling
      left_sibling_node->ChangeSizeBy(-1);

      left_sibling_guard.Drop();

    } else {
      //* get my separator and insert it with my value[0] in left sibling
      auto current_separator_key = parent_node->KeyAt(current_internal_value_index);
      left_sibling_node->Insert(current_separator_key, current_node->ValueAt(0), comparator_);

      //* insert rest of my keys/values in the left sibling
      for (int i = 1; i < current_node->GetSize(); i++) {
        left_sibling_node->Insert(current_node->KeyAt(i), current_node->ValueAt(i), comparator_);
      }

      //* remove my node and it's entry from parent
      parent_node->Delete(current_internal_value_index);

      left_sibling_guard.Drop();
      current_guard.Drop();
    }
  }

  // Recursively handle grandparent underflow
  if (parent_node->GetSize() < parent_node->GetMinSize()) {
    HandleInternalUnderflow(ctx, guard_index - 1);
  }
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE {
  if (IsEmpty()) {
    return INDEXITERATOR_TYPE();
  }

  auto guard = TraverseUntilLeftMostLeaf();

  return INDEXITERATOR_TYPE(std::move(guard), bpm_);
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  Context ctx;
  ctx.read_header_page_ = bpm_->ReadPage(header_page_id_);
  auto header_node = ctx.read_header_page_.value().As<BPlusTreeHeaderPage>();
  ctx.root_page_id_ = header_node->root_page_id_;

  //* if tree is empty return false
  if (header_node->root_page_id_ == INVALID_PAGE_ID) {
    return INDEXITERATOR_TYPE();
  }

  auto guard = TraverseUntilLeaf(key, ctx);

  return INDEXITERATOR_TYPE(std::move(guard), bpm_);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(); }

/**
 * @return Page id of the root of this tree
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t {
  auto guard = bpm_->ReadPage(header_page_id_);
  auto header_page = guard.As<BPlusTreeHeaderPage>();

  return header_page->root_page_id_;
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;

template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;

template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;

template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;

template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
