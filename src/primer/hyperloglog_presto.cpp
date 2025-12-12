//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// hyperloglog_presto.cpp
//
// Identification: src/primer/hyperloglog_presto.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/hyperloglog_presto.h"
#include <algorithm>
#include <bitset>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <type_traits>
#include "common/macros.h"

namespace bustub {

/** @brief Parameterized constructor. */
template <typename KeyType>
HyperLogLogPresto<KeyType>::HyperLogLogPresto(int16_t n_leading_bits)
    : cardinality_(0), b_(n_leading_bits < 0 ? 0 : n_leading_bits) {
  dense_bucket_ = std::vector<std::bitset<4>>(1ULL << b_, std::bitset<4>(0));
}

/** @brief Element is added for HLL calculation. */
template <typename KeyType>
auto HyperLogLogPresto<KeyType>::AddElem(KeyType val) -> void {
  /** @TODO(student) Implement this function! */

  // 1. get the hash of this value
  auto hash = CalculateHash(val);

  // 2. get it's binary representation
  auto hash_as_binary = std::bitset<64>(hash);

  // 3. get first 'b' bits of that binary and convert into a number to act as the key of the registers
  uint64_t register_key = 0;
  if (b_ > 0) {
    register_key = hash >> (64 - b_);
  }

  // 4. get the number of trailing zero and store in register as max(R[j] , p)
  uint64_t trailing_zeros = 0;
  int16_t remaining_bits = 64 - b_;
  for (int16_t i = 0; i < remaining_bits; ++i) {
    if (hash_as_binary.test(i)) {
      break;
    }
    trailing_zeros++;
  }

  auto it = registers_.find(register_key);
  bool update_buckets = false;
  if (it != registers_.end()) {
    auto max_value = std::max(it->second, trailing_zeros);

    if (trailing_zeros > it->second) {
      update_buckets = true;
    }

    registers_[register_key] = max_value;
  } else {
    registers_[register_key] = trailing_zeros;

    update_buckets = true;
  }

  // 5. store overflow & dense bits in it's buckets
  if (update_buckets) {
    overflow_bucket_[register_key] = std::bitset<OVERFLOW_BUCKET_SIZE>((trailing_zeros >> 4) & 0x07);
    dense_bucket_[register_key] = std::bitset<DENSE_BUCKET_SIZE>(trailing_zeros & 0x0F);
  }
}

/** @brief Function to compute cardinality. */
template <typename T>
auto HyperLogLogPresto<T>::ComputeCardinality() -> void {
  /** @TODO(student) Implement this function! */
  // calculate sum(1 .. m)[2^(-1 * registers[i])]
  size_t m = 1ULL << b_;

  double sum = 0.0;
  for (size_t i = 0; i < m; i++) {
    auto it = registers_.find(i);

    if (it != registers_.end()) {
      sum += std::pow(2.0, -static_cast<double>(it->second));  // 2^(-r)
    } else {
      sum += 1.0;
    }
  }

  // calculate (constant * m^2) / sum
  if (sum == 0) {
    cardinality_ = 0;
  } else {
    cardinality_ = std::floor((CONSTANT * m * m) / sum);
  }
}

template class HyperLogLogPresto<int64_t>;
template class HyperLogLogPresto<std::string>;
}  // namespace bustub
