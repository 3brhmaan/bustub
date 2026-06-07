//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// hash_join_executor.h
//
// Identification: src/include/execution/executors/hash_join_executor.h
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/logger.h"
#include "common/util/hash_util.h"
#include "execution/executor_context.h"
#include "execution/executors/abstract_executor.h"
#include "execution/plans/hash_join_plan.h"
#include "storage/table/tuple.h"

namespace bustub {

struct HashJoinKey {
  /** attributes values */
  std::vector<Value> attributes_{};

  /**
   * Compares two HashJoin keys for equality.
   * @param other the other HashJoin key to be compared with
   * @return `true` if both HashJoin keys have equivalent attributes, `false` otherwise
   */
  auto operator==(const HashJoinKey &other) const -> bool {
    for (uint32_t i = 0; i < other.attributes_.size(); i++) {
      if (attributes_[i].CompareEquals(other.attributes_[i]) != CmpBool::CmpTrue) {
        return false;
      }
    }
    return true;
  }

  auto ToString() const -> std::string {
    std::string result = "(";
    for (int i = 0; i < static_cast<int>(attributes_.size()); i++) {
      if (i + 1 == static_cast<int>(attributes_.size())) {
        result += attributes_[i].ToString() + ")";
      } else {
        result += attributes_[i].ToString() + ", ";
      }
    }

    return result;
  }
};

}  // namespace bustub

namespace std {

/** Implements std::hash on AggregateKey */
template <>
struct hash<bustub::HashJoinKey> {
  auto operator()(const bustub::HashJoinKey &hash_key) const -> std::size_t {
    size_t curr_hash = 0;
    for (const auto &key : hash_key.attributes_) {
      if (!key.IsNull()) {
        curr_hash = bustub::HashUtil::CombineHashes(curr_hash, bustub::HashUtil::HashValue(&key));
      }
    }
    return curr_hash;
  }
};

}  // namespace std

namespace bustub {
class SimpleJoinHashTable {
 public:
  void Insert(const HashJoinKey &hash_key, const Tuple &hash_val) { ht_[hash_key].push_back(hash_val); }

  auto Contains(const HashJoinKey &hash_key) -> bool { return ht_.count(hash_key) != 0U; }

  auto Get(const HashJoinKey &hash_key) -> std::vector<Tuple> {
    matched_hash_keys_.insert({hash_key, true});
    return ht_[hash_key];
  }

  /** An iterator over the hash join table */
  class Iterator {
   public:
    /** Creates an iterator for the hash join map. */
    explicit Iterator(std::unordered_map<HashJoinKey, std::vector<Tuple>>::const_iterator iter) : iter_{iter} {}

    /** @return The key of the iterator */
    auto Key() -> const HashJoinKey & { return iter_->first; }

    /** @return The value of the iterator */
    auto Val() -> const std::vector<Tuple> & { return iter_->second; }

    /** @return The iterator before it is incremented */
    auto operator++() -> Iterator & {
      ++iter_;
      return *this;
    }

    /** @return `true` if both iterators are identical */
    auto operator==(const Iterator &other) -> bool { return this->iter_ == other.iter_; }

    /** @return `true` if both iterators are different */
    auto operator!=(const Iterator &other) -> bool { return this->iter_ != other.iter_; }

   private:
    /** hash join map */
    std::unordered_map<HashJoinKey, std::vector<Tuple>>::const_iterator iter_;
  };

  /** @return Iterator to the start of the hash table */
  auto Begin() -> Iterator { return Iterator{ht_.cbegin()}; }

  /** @return Iterator to the end of the hash table */
  auto End() -> Iterator { return Iterator{ht_.cend()}; }

  // private:
  std::unordered_map<HashJoinKey, std::vector<Tuple>> ht_;

  /*represent the remain keys that never been join from right table, mainly will be used in left join*/
  std::unordered_map<HashJoinKey, bool> matched_hash_keys_;
};

/**
 * HashJoinExecutor executes a nested-loop JOIN on two tables.
 */
class HashJoinExecutor : public AbstractExecutor {
 public:
  /**
   * Construct a new HashJoinExecutor instance.
   * @param exec_ctx The executor context
   * @param plan The HashJoin join plan to be executed
   * @param left_child The child executor that produces tuples for the left side of join
   * @param right_child The child executor that produces tuples for the right side of join
   */
  HashJoinExecutor(ExecutorContext *exec_ctx, const HashJoinPlanNode *plan,
                   std::unique_ptr<AbstractExecutor> &&left_child, std::unique_ptr<AbstractExecutor> &&right_child);

  /** Initialize the join */
  void Init() override;

  /**
   * Yield the next tuple from the join.
   * @param[out] tuple The next tuple produced by the join.
   * @param[out] rid The next tuple RID, not used by hash join.
   * @return `true` if a tuple was produced, `false` if there are no more tuples.
   */
  auto Next(Tuple *tuple, RID *rid) -> bool override;

  /** @return The output schema for the join */
  auto GetOutputSchema() const -> const Schema & override { return plan_->OutputSchema(); };

 private:
  /** The HashJoin plan node to be executed. */
  const HashJoinPlanNode *plan_;

  std::unique_ptr<AbstractExecutor> left_child_;
  std::unique_ptr<AbstractExecutor> right_child_;
  std::unique_ptr<SimpleJoinHashTable> ht_;

  std::unique_ptr<SimpleJoinHashTable::Iterator> start_iter_;
  std::unique_ptr<SimpleJoinHashTable::Iterator> end_iter_;

  Tuple right_tuple_;
  std::vector<Tuple> matched_left_tuples_;
  std::vector<Tuple> left_join_tuples_;
  int left_tuples_idx_;
  int left_join_tuples_idx_;
  bool finish_left_tuples_;
};

}  // namespace bustub
