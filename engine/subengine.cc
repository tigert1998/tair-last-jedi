#include "subengine.h"

#include <libpmem.h>

#include <cstddef>
#include <mutex>
#include <tuple>

#include "config.h"
#include "utils.h"

#ifdef LOCAL_DEBUG
#define PMEM_MEMCPY(a, b, c) pmem_memcpy_persist(a, b, c)
#else
#define PMEM_MEMCPY(a, b, c) pmem_memcpy(a, b, c, PMEM_F_MEM_NONTEMPORAL)
#endif

using TP = std::chrono::high_resolution_clock::time_point;

namespace {
int GetMemUsed() {
  FILE* file = fopen("/proc/self/status", "r");
  int result = -1;
  char line[128];

  while (fgets(line, 128, file) != NULL) {
    if (strncmp(line, "VmRSS:", 6) == 0) {
      int i = strlen(line);
      const char* p = line;
      while (*p < '0' || *p > '9') p++;
      line[i - 3] = '\0';
      result = atoi(p);
      break;
    }
  }
  fclose(file);
  return result;
}

double ElapsedSeconds(const TP& from, const TP& to) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(to - from)
             .count() /
         1e3;
}
}  // namespace

TP SubEngine::key_timestamps_[3] = {};

void SubEngine::Init(int id, char* pmem_base, Logger* logger) {
  pmem_allocator_.id_ = id_ = id;
  pmem_allocator_.logger_ = logger_ = logger;

  pmem_base_ = pmem_base;

  logger_->LogWithTime("[engine #%d] SubEngine::Init()", id_);

  uint64_t pmem_frontier = hash_index_.Reconstruct(pmem_base_);

  pmem_allocator_.pmem_end_ = PMEM_SIZE_PER_SHARD;
  pmem_allocator_.set_pmem_frontier(pmem_frontier);
  pmem_allocator_.set_mode(PmemAllocator::kAppend);

  logger_->Log(
      "[engine #%d] Hash index has been reconstructed. #recovered_keys = %u",
      id_, hash_index_.num_unique_keys());
  logger_->Flush();
}

SubEngine::~SubEngine() {}

Status SubEngine::Get(const Slice& key, std::string* value) {
  auto idx = hash_index_.Find(key);
  if (idx < 0) {
    return NotFound;
  } else {
    auto pmem_record = hash_index_.FetchPmemRecord(idx);
    char* from = pmem_record->value;
    char* to = pmem_record->value + pmem_record->value_len();
    *value = std::string(from, to);

    return Ok;
  }
}

void SubEngine::RecordTimestamp(uint32_t idx) {
  constexpr uint32_t N = sizeof(key_timestamps_) / sizeof(key_timestamps_[0]);
  static std::once_flag flags[N];
  std::call_once(flags[idx], [idx]() {
    key_timestamps_[idx] = std::chrono::high_resolution_clock::now();
  });
}

void SubEngine::AdjustStrategy(uint64_t set_idx) {
  switch (set_idx) {
    case 0: {
      RecordTimestamp(0);
      break;
    }
    case SHRINK_CKPT: {
      RecordTimestamp(1);
      pmem_allocator_.set_mode(PmemAllocator::kShrink);
      break;
    }
    case RW_HYBRID_CKPT: {
      RecordTimestamp(2);
      static std::once_flag flag;
      std::call_once(flag, [this]() {
        this->logger_->LogWithTime("read/write hybrid stage begins");
        const char* stage_names[] = {"append", "shrink"};
        for (uint32_t i = 0; i <= 1; i++) {
          this->logger_->Log(
              "%s stage consumes %.3lf seconds", stage_names[i],
              ElapsedSeconds(key_timestamps_[i], key_timestamps_[i + 1]));
        }
      });
      break;
    }
    default:
      break;
  }
}

Status SubEngine::Set(const Slice& key, const Slice& value) {
  auto set_idx = num_sets_.fetch_add(1, RE);
  AdjustStrategy(set_idx);

  auto idx = hash_index_.Find(key);

  uint32_t record_size = PmemRecord::record_size(value.size());
  uint64_t ptr;
  uint32_t cap;
  std::tie(ptr, cap) = pmem_allocator_.Allocate(record_size);

  static thread_local char buf[1 << 12];

#ifdef USE_LOG
  bool is_update = (idx >= 0);
#endif

  if (idx < 0) {
    new (buf) PmemRecord(key.data(), value.data(), value.size(), cap, 0);
    PMEM_MEMCPY(pmem_base_ + ptr, buf, cap);

    if ((idx = hash_index_.Insert(key, ptr)) >= 0) {
      goto modify;
    }
  } else {
modify:
    auto previous_pmem_record = hash_index_.FetchPmemRecord(idx);
    PmemRecord* last = nullptr;
    uint64_t previous_ptr;

    do {
      new (buf) PmemRecord(key.data(), value.data(), value.size(), cap,
                           previous_pmem_record->timestamp + 1);
      PMEM_MEMCPY(pmem_base_ + ptr, buf, cap);

      previous_ptr = (char*)previous_pmem_record - pmem_base_;
      last = previous_pmem_record;
      previous_pmem_record = hash_index_.Update(idx, previous_ptr, ptr);
    } while (previous_pmem_record != last);

    pmem_allocator_.Deallocate(previous_ptr, previous_pmem_record->cap());
  }

#ifdef USE_LOG
  if ((set_idx % LOG_FREQ) == 0) {
    auto front = pmem_allocator_.free_queue_.front.load(RE);
    auto rear = pmem_allocator_.free_queue_.rear.load(RE);
    auto free_queue_size = rear - front;

    uint64_t frontier = pmem_allocator_.pmem_frontier_.load(RE);
    bool r = (pmem_allocator_.pmem_end_ > frontier);
    double remained_size = r ? (pmem_allocator_.pmem_end_ - frontier) : 0;
    remained_size = 1.0 * remained_size / (1 << 30);

    double mem_used = 1.0 * GetMemUsed() / (1 << 10);
    uint64_t num_unique_keys = hash_index_.num_unique_keys() / (1 << 10);

    logger_->Log(
        "[set #%llu] [engine #%d] #unique_keys = %lluk, len(free_queue) = "
        "%llu, remained_pmem_size = %.4fG, memory_usage = %.2fM, "
        "update = %s, len(value) = %llu",
        set_idx, id_, num_unique_keys, free_queue_size, remained_size, mem_used,
        is_update ? "true" : "false", value.size());
    logger_->Flush();
  }
#endif

  return Ok;
}
