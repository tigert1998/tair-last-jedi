#ifndef TAIR_CONTEST_KV_CONTEST_HASH_INDEX_H_
#define TAIR_CONTEST_KV_CONTEST_HASH_INDEX_H_

#include <atomic>
#include <functional>
#include <utility>

#include "common/db.h"
#include "config.h"
#include "record.h"
#include "tair_assert.h"

namespace std {
template <>
struct hash<Slice> {
  inline std::pair<uint64_t, uint8_t> operator()(
      const Slice& key) const noexcept {
    ASSERT(key.size() == KEY_SIZE);
    auto arr = (__uint64_t*)(key.data());
    uint64_t hash_value = arr[1] * HASH_P + arr[0];
    return {hash_value >> 8, hash_value & TAG_MASK};
  }
};
}  // namespace std

class HashIndex {
 private:
  MemRecord mem_records_[UNIQUE_KEYS_PER_SHARD];
  uint8_t tags_[UNIQUE_KEYS_PER_SHARD];
  std::atomic<uint32_t> num_unique_keys_;

  std::atomic<int32_t> buckets_[NUM_BUCKETS_PER_SHARD];
  std::hash<Slice> hash_func_;

  char* pmem_base_;

  void TryRecover(uint64_t ptr);

 public:
  HashIndex();

  uint64_t Reconstruct(char* pmem_base);

  int32_t Find(const Slice& key);

  int32_t Insert(const Slice& key, uint64_t ptr);

  PmemRecord* Update(uint32_t idx, uint64_t prev_ptr, uint64_t ptr);

  inline uint32_t num_unique_keys() { return num_unique_keys_.load(RE); }

  PmemRecord* FetchPmemRecord(uint32_t idx);
};

#endif