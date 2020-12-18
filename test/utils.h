#ifndef TAIR_CONTEST_KV_CONTEST_TEST_UTILS_H_
#define TAIR_CONTEST_KV_CONTEST_TEST_UTILS_H_

#include <stdint.h>

#include <string>

template <typename G>
std::string GenerateRandomString(G& g, uint32_t len) {
  std::uniform_int_distribution<uint8_t> value_dis('a', 'z');

  std::string value;
  value.resize(len);

  value[0] = '[';
  for (uint32_t i = 1; i < len - 1; i++) value[i] = value_dis(g);
  value[len - 1] = ']';
  return value;
}

#endif