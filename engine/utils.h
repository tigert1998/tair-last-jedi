#ifndef TAIR_CONTEST_KV_CONTEST_UTILS_H_
#define TAIR_CONTEST_KV_CONTEST_UTILS_H_

#include <stdint.h>

template <uint32_t B>
constexpr uint64_t Residual() {
  return ((uint64_t)1 << (uint64_t)B) - (uint64_t)1;
}

template <uint32_t B>
constexpr uint64_t Align(const uint64_t num) {
  return (num + Residual<B>()) & (~Residual<B>());
}

#endif