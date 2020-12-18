#include "engine.h"

#include <libpmem.h>
#include <sys/stat.h>

#include <cstddef>
#include <mutex>
#include <tuple>

#include "config.h"

Status DB::CreateOrOpen(const std::string& name, DB** dbptr, FILE* log_file) {
  return Engine::CreateOrOpen(name, dbptr, log_file);
}

DB::~DB() {}

Status Engine::CreateOrOpen(const std::string& name, DB** dbptr,
                            FILE* log_file) {
  *dbptr = new Engine(name, log_file);
  return Ok;
}

Engine::Engine(const std::string& name, FILE* log_file) {
  logger_.reset(new Logger(log_file));
  logger_->LogWithTime("Engine::Engine()");
  pmem_base_ = InitializeDB(name);
  logger_->LogWithTime("db file at \"%s\" has been opened", name.c_str());

  for (uint32_t i = 0; i < NUM_SHARDS; i++) {
    engines_[i].Init(i, pmem_base_ + PMEM_SIZE_PER_SHARD * i, logger_.get());
  }

  logger_->Log("sizeof(PmemRecord) = %d", sizeof(PmemRecord));
  logger_->Log("sizeof(MemRecord) = %d", sizeof(MemRecord));
  logger_->Log("is_pmem = %s", is_pmem_ ? "true" : "false");
  logger_->Log("pmem_has_auto_flush = %s",
               pmem_has_auto_flush() ? "true" : "false");
  logger_->Log("pmem_has_hw_drain = %s",
               pmem_has_hw_drain() ? "true" : "false");
  logger_->Flush();
}

Status Engine::Get(const Slice& key, std::string* value) {
  uint32_t idx = key.data()[0] & SHARD_HASH_MASK;
  return engines_[idx].Get(key, value);
}

Status Engine::Set(const Slice& key, const Slice& value) {
  uint32_t idx = key.data()[0] & SHARD_HASH_MASK;
  return engines_[idx].Set(key, value);
}

Engine::~Engine() { pmem_unmap(pmem_base_, mapped_len_); }

char* Engine::InitializeDB(const std::string& path) {
  struct stat buffer;
  bool exist = stat(path.c_str(), &buffer) == 0;

  auto ptr = (char*)pmem_map_file(path.c_str(), PMEM_SIZE, PMEM_FILE_CREATE,
                                  0666, &mapped_len_, &is_pmem_);
  if (exist) return ptr;

  // initialize as 0
  uint64_t size_per_thread = PMEM_SIZE / NUM_THREADS;
  static_assert(PMEM_SIZE % NUM_THREADS == 0,
                "workload should be evenly distributed");
  for (uint32_t i = 0; i < NUM_THREADS; i++) {
    threads_[i] = std::thread(
        [](char* ptr, uint64_t size) { pmem_memset_persist(ptr, 0, size); },
        ptr + size_per_thread * i, size_per_thread);
  }
  for (uint32_t i = 0; i < NUM_THREADS; i++) {
    threads_[i].join();
  }
  return ptr;
}