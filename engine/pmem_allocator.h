#ifndef TAIR_CONTEST_KV_CONTEST_PMEM_ALLOCATOR_H_
#define TAIR_CONTEST_KV_CONTEST_PMEM_ALLOCATOR_H_

#include <stdint.h>

#include <atomic>
#include <utility>

#include "config.h"
#include "logger.h"
#include "record.h"
#include "utils.h"

class PmemAllocator {
  friend class Engine;
  friend class SubEngine;

 public:
  enum Mode : uint8_t {
    kAppend,
    kShrink,
  };

  PmemAllocator();

  void set_pmem_frontier(uint64_t pmem_frontier);

  void set_mode(Mode mode);

  // returns (ptr, cap)
  std::tuple<uint64_t, uint32_t> Allocate(uint32_t size);

  void Deallocate(uint64_t ptr, uint32_t cap);

 private:
  int id_;
  static const uint32_t NUM_HEADS =
      Align<ADDRESS_ALIGN_BITS>(PmemRecord::max_record_size());

  uint64_t pmem_start_, pmem_end_;
  std::atomic<uint64_t> pmem_frontier_;
  std::atomic<Mode> mode_;

  struct FreeQueue {
    uint32_t data[GC_POOL_SIZE_PER_SHARD];
    std::atomic<uint64_t> front, rear;

    uint32_t PopFront();
    void PushBack(uint32_t item);
  };

  struct MemoryRange {
    uint64_t ptr;
    std::atomic<int32_t> next;
  };

  FreeQueue free_queue_;
  MemoryRange pool_[GC_POOL_SIZE_PER_SHARD];
  std::atomic<int32_t> heads_[NUM_HEADS + 1];
  Logger* logger_;

  bool TryAllocate(uint32_t cap, uint64_t* ptr);

  std::tuple<bool, uint64_t, uint32_t> InternalAllocate(uint32_t min_cap);
  std::tuple<uint64_t, uint32_t> AppendAllocate(uint32_t cap);
};

#endif