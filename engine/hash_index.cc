#include "hash_index.h"

#include <algorithm>

HashIndex::HashIndex() {
  auto buckets_ptr = (int32_t*)buckets_;
  std::fill(buckets_ptr, buckets_ptr + NUM_BUCKETS_PER_SHARD, -1);
}

void HashIndex::TryRecover(uint64_t ptr) {
  auto pmem_record = (PmemRecord*)(pmem_base_ + ptr);
  Slice key(pmem_record->key, KEY_SIZE);
  int32_t idx = Find(key);

  if (idx < 0) {
    Insert(key, ptr);
  } else {
    PmemRecord* previous_pmem_record = FetchPmemRecord(idx);
    if (previous_pmem_record->timestamp < pmem_record->timestamp) {
      mem_records_[idx].ptr.store(ptr, RE);
    }
  }
}

uint64_t HashIndex::Reconstruct(char* pmem_base) {
  pmem_base_ = pmem_base;

  uint64_t pmem_frontier = 0;

  int64_t last_zero_ptr = -1;
  for (uint64_t ptr = 0; ptr < PMEM_SIZE_PER_SHARD; ptr++) {
    if (pmem_base_[ptr] == 0 && last_zero_ptr < 0) {
      last_zero_ptr = ptr;
    } else if (pmem_base_[ptr] != 0) {
      last_zero_ptr = -1;
    }
    if (last_zero_ptr >= 0 &&
        ptr > (uint64_t)last_zero_ptr + RECOVER_MAX_BLANK_SIZE) {
      break;
    }

    auto pmem_record = (PmemRecord*)(pmem_base_ + ptr);
    if (ptr + pmem_record->record_size() <= PMEM_SIZE_PER_SHARD &&
        pmem_record->Intact()) {
      TryRecover(ptr);
      pmem_frontier = ptr + pmem_record->record_size();
    }
  }

  return pmem_frontier;
}

int32_t HashIndex::Find(const Slice& key) {
  uint64_t hash_value;
  uint8_t tag;
  std::tie(hash_value, tag) = hash_func_(key);
  uint32_t bucket_idx = hash_value % NUM_BUCKETS_PER_SHARD;

  for (int32_t node = buckets_[bucket_idx].load(RE); node >= 0;
       node = mem_records_[node].next) {
    if (tag != tags_[node]) {
      continue;
    }
    auto pmem_record = FetchPmemRecord(node);

    if (memcmp(pmem_record->key, key.data(), KEY_SIZE) == 0) {
      return node;
    }
  }

  return -1;
}

int32_t HashIndex::Insert(const Slice& key, uint64_t ptr) {
  uint32_t node = num_unique_keys_.fetch_add(1, RE);

  uint64_t hash_value;
  uint8_t tag;
  std::tie(hash_value, tag) = hash_func_(key);
  uint32_t bucket_idx = hash_value % NUM_BUCKETS_PER_SHARD;

  tags_[node] = tag;
  mem_records_[node].ptr = ptr;

  int32_t head = buckets_[bucket_idx].load(RE);
  int32_t tail = -1;
  while (1) {
    for (int32_t i = head; i != tail; i = mem_records_[i].next) {
      if (tags_[i] == tag) {
        if (0 == memcpy(FetchPmemRecord(i)->key, key.data(), KEY_SIZE)) {
          return i;
        }
      }
    }

    mem_records_[node].next = head;
    tail = head;

    if (buckets_[bucket_idx].compare_exchange_strong(head, node, RE, RE)) {
      break;
    }
  }

  return -1;
}

PmemRecord* HashIndex::Update(uint32_t idx, uint64_t prev_ptr, uint64_t ptr) {
  uint32_t prev_ptr_32b = prev_ptr;
  mem_records_[idx].ptr.compare_exchange_strong(prev_ptr_32b, ptr, RE, RE);
  return (PmemRecord*)(prev_ptr_32b + pmem_base_);
}

PmemRecord* HashIndex::FetchPmemRecord(uint32_t idx) {
  return (PmemRecord*)(mem_records_[idx].ptr.load(RE) + pmem_base_);
}