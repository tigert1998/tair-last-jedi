#include "pmem_allocator.h"

#include <algorithm>
#include <mutex>

#include "utils.h"

using std::make_tuple;

uint32_t PmemAllocator::FreeQueue::PopFront() {
  auto idx = front.fetch_add(1, RE);
  return data[idx % GC_POOL_SIZE_PER_SHARD];
}

void PmemAllocator::FreeQueue::PushBack(uint32_t item) {
  auto idx = rear.fetch_add(1, RE);
  data[idx % GC_POOL_SIZE_PER_SHARD] = item;
}

PmemAllocator::PmemAllocator() {
  auto heads_ptr = (int32_t *)heads_;
  std::fill(heads_ptr, heads_ptr + NUM_HEADS + 1, -1);
  for (uint32_t i = 0; i < GC_POOL_SIZE_PER_SHARD; i++) {
    free_queue_.data[i] = i;
  }
  free_queue_.front.store(0, RE);
  free_queue_.rear.store(GC_POOL_SIZE_PER_SHARD, RE);
}

void PmemAllocator::set_pmem_frontier(uint64_t pmem_frontier) {
  pmem_frontier_.store(pmem_frontier, RE);
}

void PmemAllocator::set_mode(Mode mode) {
  mode_.store(mode, RE);
  static std::atomic<uint32_t> counts[2] = {{0}, {0}};
  uint32_t rank = counts[mode].fetch_add(1, RE);

  const char *mode_str;
  switch (mode) {
    default:
    case kAppend: {
      mode_str = "append";
      break;
    }
    case kShrink: {
      mode_str = "shrink";
      break;
    }
  }

  if (rank == 0 || rank == NUM_SHARDS - 1) {
    logger_->LogWithTime("[engine #%d] [rank #%u] set allocator mode to \"%s\"",
                         id_, rank, mode_str);
    logger_->Flush();
  }
}

void PmemAllocator::Deallocate(uint64_t ptr, uint32_t cap) {
  std::atomic<int32_t> *head = heads_ + cap;

  uint32_t idx = free_queue_.PopFront();
  pool_[idx].ptr = ptr;

  // insert
  int32_t next = head->load(RE);
  while (1) {
    *(uint32_t *)&pool_[idx].next = next;
    if (head->compare_exchange_strong(next, idx)) {
      break;
    }
  }
}

bool PmemAllocator::TryAllocate(uint32_t cap, uint64_t *ptr) {
  int32_t idx = heads_[cap].load(RE);
  if (idx < 0) {
    return false;
  }
  int32_t next = pool_[idx].next.load(RE);

  while (1) {
    if (heads_[cap].compare_exchange_strong(idx, next)) {
      *ptr = pool_[idx].ptr;
      free_queue_.PushBack(idx);
      return true;
    }
    if (idx < 0) {
      return false;
    }
    next = pool_[idx].next.load(RE);
  }

  return false;
}

std::tuple<bool, uint64_t, uint32_t> PmemAllocator::InternalAllocate(
    uint32_t min_cap) {
  uint64_t ptr;
  for (uint32_t cap = min_cap; cap <= NUM_HEADS; cap += ADDRESS_ALIGN_NUM) {
    if (TryAllocate(cap, &ptr)) {
      return make_tuple(true, ptr, cap);
    }
  }

  return make_tuple(false, 0, 0);
}

std::tuple<uint64_t, uint32_t> PmemAllocator::Allocate(uint32_t size) {
  size = Align<ADDRESS_ALIGN_BITS>(size);

  Mode mode = mode_.load(RE);
  switch (mode) {
    case kAppend: {
      goto use_append;
    }
    case kShrink: {
      bool found;
      uint64_t ptr;
      uint32_t cap;
      std::tie(found, ptr, cap) = InternalAllocate(size);
      if (found) {
        if (cap < size + PmemRecord::min_record_size()) {
          return make_tuple(ptr, cap);
        } else {
          Deallocate(ptr + size, cap - size);
          return make_tuple(ptr, size);
        }
      } else {
        goto use_append;
      }
    }
  }

use_append:;
  uint64_t ptr;
  uint32_t cap;
  std::tie(ptr, cap) = AppendAllocate(size);
  return make_tuple(ptr, cap);
}

std::tuple<uint64_t, uint32_t> PmemAllocator::AppendAllocate(uint32_t cap) {
  uint64_t ptr = pmem_frontier_.fetch_add(cap, RE);
  return make_tuple(ptr, cap);
}