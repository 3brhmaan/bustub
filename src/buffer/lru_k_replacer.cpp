//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"
#include <cstddef>
#include <optional>
#include <ostream>
#include "common/config.h"
#include "common/exception.h"
#include "common/macros.h"

namespace bustub {

auto CurrentTimestampe() -> size_t {
  auto now = std::chrono::system_clock::now();

  auto ms = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

  auto timestamp_ns = static_cast<size_t>(ms);

  return timestamp_ns;
}

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {}

auto LRUKReplacer::Evict() -> std::optional<frame_id_t> {
  std::scoped_lock slk(latch_);

  if (node_equal_k_.empty() && node_less_k_.empty()) {
    return std::nullopt;
  }

  frame_id_t frame_id = -1;

  if (!node_less_k_.empty()) {
    auto node_it = node_less_k_.begin();
    frame_id = node_it->GetFrameId();

    node_store_.erase(node_store_.find(node_it->GetFrameId()));
    node_less_k_.erase(node_it);
    curr_size_--;

  } else {
    auto node_it = node_equal_k_.begin();
    frame_id = node_it->GetFrameId();

    node_store_.erase(node_store_.find(node_it->GetFrameId()));
    node_equal_k_.erase(node_it);
    curr_size_--;
  }

  return std::optional<frame_id_t>{frame_id};
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id, [[maybe_unused]] AccessType access_type) {
  std::scoped_lock slk(latch_);

  //* check if id is valid (id < replace_szie), if not abort
  BUSTUB_ASSERT(static_cast<size_t>(frame_id) <= replacer_size_, "ERR: Frame ID is larger than Replacer Size");

  //* get the current timestamp
  auto timestamp_ns = CurrentTimestampe();

  auto it = node_store_.find(frame_id);
  if (it != node_store_.end()) {
    //* update the frame with that id with the new timestamp, which will
    //* be added at the first
    LRUKNode &node = it->second;

    //* check if node is evictable , remove the node then update it then re-insert it
    //* if not evictable , just update the node with the timestamp
    if (node.is_evictable_) {
      if (node.Size() < k_) {
        node_less_k_.erase(node);
      } else {
        node_equal_k_.erase(node);
      }

      node.AddTimestamp(timestamp_ns);

      if (node.Size() < k_) {
        node_less_k_.insert(node);
      } else {
        node_equal_k_.insert(node);
      }
    } else {
      node.AddTimestamp(timestamp_ns);
    }

  } else {
    //* add new frame if it doesn't exist and add timestamp to it
    LRUKNode node{frame_id, k_};
    node.AddTimestamp(timestamp_ns);

    node_store_[frame_id] = node;
  }
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::scoped_lock slk(latch_);

  //* check if the id is valid
  BUSTUB_ASSERT(static_cast<size_t>(frame_id) <= replacer_size_, "ERR: Frame Id > Replacer Size");

  //* if frame doesn't exist , then just return and do nothing
  if (node_store_.count(frame_id) != 1) {
    return;
  }

  //* get the node and change it's eviction status if different
  //* and return if same status
  auto &node = node_store_.find(frame_id)->second;
  if (node.is_evictable_ == set_evictable) {
    return;
  }

  node.is_evictable_ = set_evictable;
  //* change numeber of evictable pages according to set_victable
  if (set_evictable) {
    curr_size_++;
  } else {
    curr_size_--;
  }

  //* according to 'set_evictable' insert/erase frame from page LRU-K Tracker
  if (set_evictable) {
    if (node.Size() < k_) {
      node_less_k_.insert(node);
    } else {
      node_equal_k_.insert(node);
    }
  } else {
    if (node.Size() < k_) {
      node_less_k_.erase(node);
    } else {
      node_equal_k_.erase(node);
    }
  }
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  std::scoped_lock slk(latch_);

  //* check id is valid or not
  BUSTUB_ASSERT(static_cast<size_t>(frame_id) <= replacer_size_, "ERR: Frame Id > Replacer Size");

  if (node_store_.count(frame_id) != 1) {
    return;
  }

  BUSTUB_ASSERT(node_store_[frame_id].is_evictable_ == true, "ERR: Frame isn't evictable");

  //* remove the node
  auto &node = node_store_[frame_id];

  if (node.Size() < k_) {
    node_less_k_.erase(node);
  } else {
    node_equal_k_.erase(node);
  }

  node_store_.erase(node_store_.find(frame_id));

  curr_size_--;
}

auto LRUKReplacer::Size() -> size_t {
  std::scoped_lock slk(latch_);

  return curr_size_;
}

}  // namespace bustub
