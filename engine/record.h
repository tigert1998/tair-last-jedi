#ifndef TAIR_CONTEST_KV_CONTEST_RECORD_H_
#define TAIR_CONTEST_KV_CONTEST_RECORD_H_

#include <atomic>

#include "common/db.h"
#include "config.h"

struct __attribute__((packed)) PmemRecord {
  static constexpr uint32_t HEAD_BITS = 8;
  static constexpr uint32_t VALUE_LEN_BITS = 10;
  static constexpr uint32_t CAP_BITS = 11;
  static constexpr uint32_t DIGEST_BITS = 9;
  static constexpr uint32_t TIMESTAMP_BITS = 10;

  static_assert((HEAD_BITS + VALUE_LEN_BITS + CAP_BITS + DIGEST_BITS +
                 TIMESTAMP_BITS) %
                        8 ==
                    0,
                "PmemRecord head not aligned");

 public:
  uint16_t head : HEAD_BITS;

 private:
  // length of value
  uint16_t value_len_ : VALUE_LEN_BITS;
  // total capacity of the whole record
  uint16_t cap_ : CAP_BITS;

 public:
  uint16_t digest : DIGEST_BITS;
  uint16_t timestamp : TIMESTAMP_BITS;
  char key[KEY_SIZE];
  char value[80];

  PmemRecord(char *key, char *value, uint32_t value_len, uint32_t cap,
             uint32_t timestamp);
  bool Intact();

  inline uint32_t value_len() { return 80 + this->value_len_; }
  inline uint32_t set_value_len(uint32_t value_len) {
    return this->value_len_ = value_len - 80;
  }
  inline uint32_t cap() { return 1 + this->cap_; }
  inline uint32_t set_cap(uint32_t cap) { return this->cap_ = cap - 1; }

  uint32_t record_size();

  constexpr static uint32_t min_record_size() {
    return PmemRecord::record_size(80);
  }
  constexpr static uint32_t max_record_size() {
    return PmemRecord::record_size(1024);
  }
  constexpr static uint32_t record_size(uint32_t value_len) {
    return sizeof(PmemRecord) - sizeof(value) + value_len;
  }

  static uint16_t CalcDigest(char *key, char *value, uint32_t value_len,
                             uint32_t cap, uint32_t timestamp);
};

struct MemRecord {
  int32_t next;
  std::atomic<uint32_t> ptr;

#ifndef LOCAL_DEBUG
  static_assert(NUM_SHARDS >= 64,
                "ptr is not sufficient to reference this scale");
#endif
};

#endif
