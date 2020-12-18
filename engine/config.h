#ifndef TAIR_CONTEST_KV_CONTEST_CONFIG_H_
#define TAIR_CONTEST_KV_CONTEST_CONFIG_H_

#include <stdint.h>

#include <atomic>

#define USE_LOG

constexpr std::memory_order RE = std::memory_order_relaxed;

const uint64_t NUM_THREADS = 16;
const uint64_t KEY_SIZE = 16;
const uint64_t HASH_P = 199;
const uint64_t TAG_MASK = (1 << 8) - 1;

const uint32_t NUM_SHARDS = 64;
const uint8_t SHARD_HASH_MASK = NUM_SHARDS - 1;
static_assert((NUM_SHARDS & (-NUM_SHARDS)) == NUM_SHARDS,
              "NUM_SHARDS should be 2^n");

#ifdef LOCAL_DEBUG
const uint64_t PMEM_SIZE = 16 * (1 << 20);
const uint64_t NUM_KEYS = NUM_THREADS * 1000;
const uint64_t LOG_FREQ = 1;
#else
const uint64_t PMEM_SIZE = 64ull * (1ull << 30);
const uint64_t NUM_KEYS = NUM_THREADS * 24 * (1 << 20);
const uint64_t LOG_FREQ = 1 << 20;
#endif

const double UNIQUE_KEYS_RATIO = 0.6;

const uint64_t KEYS_PER_SHARD = NUM_KEYS / NUM_SHARDS;
const uint64_t UNIQUE_KEYS_PER_SHARD = KEYS_PER_SHARD * UNIQUE_KEYS_RATIO;
const uint64_t PMEM_SIZE_PER_SHARD = PMEM_SIZE / NUM_SHARDS;

const uint64_t GC_POOL_SIZE_PER_SHARD = UNIQUE_KEYS_PER_SHARD;
const uint64_t NUM_BUCKETS_PER_SHARD = KEYS_PER_SHARD;

const uint32_t ADDRESS_ALIGN_BITS = 6;
const uint32_t ADDRESS_ALIGN_NUM = (1 << ADDRESS_ALIGN_BITS);

const uint64_t SHRINK_CKPT = 220200960 / NUM_SHARDS;
const uint64_t RW_HYBRID_CKPT = NUM_KEYS / NUM_SHARDS;

const uint8_t PMEM_RECORD_HEAD = 1;
const uint32_t RECOVER_MAX_BLANK_SIZE = 4 * (1 << 10);

#endif
