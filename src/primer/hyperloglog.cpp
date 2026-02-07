//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// hyperloglog.cpp
//
// Identification: src/primer/hyperloglog.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/hyperloglog.h"
#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <ostream>

namespace bustub {

/** @brief Parameterized constructor. */
template <typename KeyType>
HyperLogLog<KeyType>::HyperLogLog(int16_t n_bits) : cardinality_(0), b_(n_bits < 0 ? 0 : n_bits) {}

/**
 * @brief Function that computes binary.
 *
 * @param[in] hash
 * @returns binary of a given hash
 */
template <typename KeyType>
auto HyperLogLog<KeyType>::ComputeBinary(const hash_t &hash) const -> std::bitset<BITSET_CAPACITY> {
  /** @TODO(student) Implement this function! */

  return {hash};
}

/**
 * @brief Function that computes leading zeros.
 *
 * @param[in] bset - binary values of a given bitset
 * @returns leading zeros of given binary set
 */
template <typename KeyType>
auto HyperLogLog<KeyType>::PositionOfLeftmostOne(const std::bitset<BITSET_CAPACITY> &bset) const -> uint64_t {
  /** @TODO(student) Implement this function! */

  // b = 3
  //    63 62 61 60 59 ...
  // 0b 0  1  1  0  1  ...

  uint64_t msp_position = 0;
  for (int i = 63 - b_; i >= 0; i--) {
    msp_position++;

    if (bset.test(i)) {
      break;
    }
  }

  return msp_position;
}

/**
 * @brief Adds a value into the HyperLogLog.
 *
 * @param[in] val - value that's added into hyperloglog
 */
template <typename KeyType>
auto HyperLogLog<KeyType>::AddElem(KeyType val) -> void {
  /** @TODO(student) Implement this function! */

  // 1. get the hash of this value
  auto hash = CalculateHash(val);

  // 2. get it's binary representation
  auto hash_as_binary = ComputeBinary(hash);

  // 3. get first 'b' bits of that binary and convert into a number to act as the key of the registers
  uint64_t register_key = hash >> (64 - b_);
  if (b_ == 0) {
    register_key = 0;
  }

  // 4. get the 'MSB' position from that binary and store in the register as 'max(R[j] , p)'
  auto msp_position = PositionOfLeftmostOne(hash_as_binary);

  auto it = registers_.find(register_key);
  if (it != registers_.end()) {
    auto max_msp_position = std::max(it->second, msp_position);

    registers_[register_key] = max_msp_position;
  } else {
    registers_[register_key] = msp_position;
  }
}

/**
 * @brief Function that computes cardinality.
 */
template <typename KeyType>
auto HyperLogLog<KeyType>::ComputeCardinality() -> void {
  /** @TODO(student) Implement this function! */

  // calculate sum(1 .. m)[2^(-1 * registers[i])]
  size_t m = 1ULL << b_;

  double sum = 0.0;
  for (size_t i = 0; i < m; i++) {
    auto it = registers_.find(i);

    if (it != registers_.end()) {
      sum += 1.0 / (1ULL << it->second);  // 2^(-r) = 1 / (2^r)
    } else {
      sum += 1.0;
    }
  }

  // calculate (constant * m^2) / sum
  if (sum == 0) {
    cardinality_ = 0;
  } else {
    cardinality_ = std::floor((CONSTANT * 1.0 * m * m) / sum);
  }
}

template class HyperLogLog<int64_t>;
template class HyperLogLog<std::string>;

}  // namespace bustub
