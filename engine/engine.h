#ifndef TAIR_CONTEST_KV_CONTEST_ENGINE_H_
#define TAIR_CONTEST_KV_CONTEST_ENGINE_H_

#include <stdint.h>

#include <atomic>
#include <memory>
#include <thread>

#include "hash_index.h"
#include "logger.h"
#include "pmem_allocator.h"
#include "subengine.h"

class Engine : DB {
 public:
  static Status CreateOrOpen(const std::string& name, DB** dbptr,
                             FILE* log_file);

  Engine(const std::string& name, FILE* log_file);

  Status Get(const Slice& key, std::string* value);

  Status Set(const Slice& key, const Slice& value);

  ~Engine();

 private:
  std::unique_ptr<Logger> logger_;
  char* pmem_base_;
  uint64_t mapped_len_;
  int is_pmem_;
  PmemRecord* pmem_records_;
  std::thread threads_[NUM_THREADS];

  SubEngine engines_[NUM_SHARDS];

  char* InitializeDB(const std::string& path);
};

#endif
