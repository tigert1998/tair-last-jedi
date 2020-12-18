#include "record.h"

#include <x86intrin.h>

#include "utils.h"

bool PmemRecord::Intact() {
  if (head != PMEM_RECORD_HEAD) return false;
  if (!(80 <= value_len() && value_len() <= 1024)) return false;
  if (this->record_size() > cap()) return false;
  bool ret = CalcDigest(key, value, value_len(), cap(), timestamp) == digest;
  return ret;
}

uint16_t PmemRecord::CalcDigest(char *key, char *value, uint32_t value_len,
                                uint32_t cap, uint32_t timestamp) {
#ifdef USE_STRICT_DIGEST
  // strict check code
  // TODO: optimize me
  static const uint16_t rand_nums[256] = {
      58165, 24872, 20215, 53706, 1132,  24634, 48229, 65249, 39820, 10409,
      27591, 17994, 27430, 58691, 64967, 55141, 36450, 61067, 59359, 18763,
      53580, 50980, 32370, 13571, 64234, 56080, 22434, 8247,  48041, 44685,
      38463, 18533, 26818, 21865, 59516, 24323, 22244, 9449,  55171, 47736,
      11955, 57941, 60054, 55184, 19465, 7117,  30039, 15525, 62244, 19,
      32875, 54745, 4709,  42085, 61426, 24236, 8404,  25581, 42755, 16888,
      8428,  58339, 22072, 61108, 17030, 7024,  16904, 25338, 46617, 37793,
      11114, 22349, 37962, 41773, 33855, 34883, 52508, 19882, 10602, 1909,
      45033, 48919, 48315, 12205, 6550,  58810, 59528, 20902, 51116, 41850,
      51390, 15890, 52384, 5553,  32094, 30069, 35892, 62464, 62274, 11622,
      53021, 5085,  49322, 60607, 30523, 56064, 11558, 254,   54143, 13793,
      11253, 4080,  50069, 22243, 13840, 22256, 27809, 26901, 8423,  27096,
      50816, 64970, 8089,  43023, 32872, 16654, 50556, 46584, 36399, 9455,
      2681,  55553, 63272, 42957, 46527, 32233, 15229, 33571, 2566,  63447,
      24658, 1533,  29233, 3169,  29082, 45811, 32029, 11814, 25874, 13903,
      23299, 7814,  8180,  37555, 5598,  4289,  38910, 29994, 44796, 58808,
      5041,  51026, 6973,  8283,  52608, 34470, 47282, 53835, 3115,  41741,
      65092, 33596, 25608, 34755, 56297, 56397, 59025, 45044, 46934, 23353,
      30867, 9598,  12448, 42933, 17309, 21636, 14642, 7922,  40701, 14021,
      60918, 61073, 15984, 4084,  46424, 38174, 27788, 29473, 14530, 34816,
      45465, 15245, 17467, 26772, 49316, 52081, 9777,  63387, 63919, 53352,
      51073, 960,   53768, 29130, 21202, 57465, 33729, 22530, 53681, 13434,
      7966,  13672, 42108, 10243, 17572, 59644, 57778, 63777, 45970, 54015,
      33650, 3896,  33540, 13287, 61516, 19354, 40449, 46739, 40029, 17301,
      41840, 36206, 62468, 46758, 12264, 52353, 10870, 63841, 30228, 28478,
      18603, 61683, 53799, 58213, 62959, 3665};

  uint16_t code = 0;
  uint16_t *key_arr = reinterpret_cast<uint16_t *>(key);
  uint16_t *value_arr = reinterpret_cast<uint16_t *>(value);
  value_len = std::min((uint16_t)1024, value_len);
  code += key_arr[0] + key_arr[2] * 3 + key_arr[1] * 5 + key_arr[3] * 7;
  uint16_t len = std::min(value_len / 4, 256);
  for (int i = 0; i < len; i += 4) {
    code += (value_arr[i] ^ rand_nums[i]);
  }
  code = code + timestamp + 9;
  return code & ((1 << DIGEST_BITS) - 1);

#else
  return (*(uint64_t *)key +
          *(uint64_t *)(value + value_len - sizeof(uint64_t)) +
          (value_len >> 3) + (cap >> 3) + timestamp) &
         ((1 << DIGEST_BITS) - 1);
#endif
}

PmemRecord::PmemRecord(char *key, char *value, uint32_t value_len, uint32_t cap,
                       uint32_t timestamp) {
  this->head = PMEM_RECORD_HEAD;
  set_value_len(value_len);
  set_cap(cap);
  this->digest =
      CalcDigest(key, value, this->value_len(), this->cap(), timestamp);
  this->timestamp = timestamp;
  memcpy(this->key, key, KEY_SIZE);
  memcpy(this->value, value, value_len);
}

uint32_t PmemRecord::record_size() {
  return PmemRecord::record_size(value_len());
}
