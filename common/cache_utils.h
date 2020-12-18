#include <x86intrin.h>

#define clflush(addr) _mm_clflush(addr)
#define clflushopt(addr) _mm_clflushopt(addr)
#define clwb(addr) _mm_clwb(addr)
#define lfence() _mm_lfence()
#define sfence() _mm_sfence()
#define mfence() _mm_mfence()