#ifndef TAIR_CONTEST_KV_CONTEST_TAIR_ASSERT_H_
#define TAIR_CONTEST_KV_CONTEST_TAIR_ASSERT_H_

#ifdef LOCAL_DEBUG
#define ASSERT(cond)                             \
  if (!(cond)) {                                 \
    printf("assertion failed: \"" #cond "\"\n"); \
    fflush(stdout);                              \
    exit(1);                                     \
  }
#else
#define ASSERT(cond) (cond)
#endif

#endif