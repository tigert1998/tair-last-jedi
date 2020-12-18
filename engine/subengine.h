#ifndef TAIR_CONTEST_KV_CONTEST_SUBENGINE_H_
#define TAIR_CONTEST_KV_CONTEST_SUBENGINE_H_

#include <stdint.h>

#include <atomic>
#include <chrono>

#include "hash_index.h"
#include "logger.h"
#include "pmem_allocator.h"

class SubEngine {
 public:
  SubEngine() = default;

  void Init(int id, char* pmem_base, Logger* logger);

  Status Get(const Slice& key, std::string* value);

  Status Set(const Slice& key, const Slice& value);

  ~SubEngine();

 private:
  int id_;

  Logger* logger_;
  char* pmem_base_;

  HashIndex hash_index_;
  PmemAllocator pmem_allocator_;

  // #sets
  std::atomic<uint64_t> num_sets_;

  static std::chrono::high_resolution_clock::time_point key_timestamps_[3];

  void RecordTimestamp(uint32_t idx);
  void AdjustStrategy(uint64_t set_idx);
};

#endif
