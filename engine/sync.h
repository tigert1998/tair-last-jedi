#ifndef TAIR_CONTEST_KV_CONTEST_SYNC_H_
#define TAIR_CONTEST_KV_CONTEST_SYNC_H_

#include <pthread.h>
#include <semaphore.h>

#include <atomic>
#include <condition_variable>

#include "tair_assert.h"

class WaitGroup {
 public:
  void Add(int delta) { cnt_ += delta; }

  void Done() {
    cnt_--;
    if (cnt_ <= 0) cond_.notify_all();
  }

  void Wait() {
    std::unique_lock<std::mutex> lock(mtx_);
    cond_.wait(lock, [&] { return cnt_ <= 0; });
  }

 private:
  std::mutex mtx_;
  std::atomic<int> cnt_;
  std::condition_variable cond_;
};

class SpinMutex {
 private:
  std::atomic_flag flag_;

 public:
  inline void lock() {
    while (flag_.test_and_set(std::memory_order_acquire))
      ;
  }

  inline void unlock() { flag_.clear(std::memory_order_release); }

  inline bool try_lock() {
    return !flag_.test_and_set(std::memory_order_acquire);
  }
};

class SpinSharedMutex {
 private:
  std::atomic<int8_t> cnt_;

 public:
  inline void lock() {
    int8_t expected = 0;
    while (!cnt_.compare_exchange_strong(
        expected, -1, std::memory_order_acquire, std::memory_order_relaxed)) {
      expected = 0;
    }
  }

  inline void lock_shared() {
    int8_t expected = -1;
    while (1) {
      while (expected < 0) expected = cnt_.load(std::memory_order_relaxed);
      if (cnt_.compare_exchange_strong(expected, expected + 1,
                                       std::memory_order_acquire,
                                       std::memory_order_relaxed)) {
        return;
      }
    }
  }

  inline void unlock() { cnt_.store(0, std::memory_order_release); }

  inline void unlock_shared() { cnt_.fetch_sub(1, std::memory_order_release); }

  inline bool try_lock() {
    int8_t expected = 0;
    return cnt_.compare_exchange_strong(expected, -1, std::memory_order_acquire,
                                        std::memory_order_relaxed);
  }
};

class SharedMutex {
 private:
  pthread_rwlock_t mtx_;

 public:
  inline SharedMutex() : mtx_(PTHREAD_RWLOCK_INITIALIZER) {}

  inline ~SharedMutex() { ASSERT(pthread_rwlock_destroy(&mtx_) == 0); }

  SharedMutex(const SharedMutex&) = delete;

  SharedMutex& operator=(const SharedMutex&) = delete;

  inline void lock() { ASSERT(pthread_rwlock_wrlock(&mtx_) == 0); }

  inline void lock_shared() { ASSERT(pthread_rwlock_rdlock(&mtx_) == 0); }

  inline bool try_lock() { return pthread_rwlock_trywrlock(&mtx_) == 0; }

  inline bool try_lock_shared() { return pthread_rwlock_tryrdlock(&mtx_) == 0; }

  inline void unlock() { ASSERT(0 == pthread_rwlock_unlock(&mtx_)); }

  inline void unlock_shared() { this->unlock(); }
};

template <typename MutexType>
class SharedLock {
 private:
  MutexType* mtx_ptr_;

 public:
  explicit SharedLock(MutexType& m) : mtx_ptr_(&m) { Lock(); }

  void Lock() { mtx_ptr_->lock_shared(); }

  void Unlock() { mtx_ptr_->unlock_shared(); }

  ~SharedLock() { Unlock(); }
};

class Semaphore {
 private:
  sem_t sem_;

 public:
  inline Semaphore(uint32_t value) {
    // shared between threads
    ASSERT(0 == sem_init(&sem_, 0, value));
  }

  inline ~Semaphore() { ASSERT(0 == sem_destroy(&sem_)); }

  inline void Wait() { ASSERT(0 == sem_wait(&sem_)); }

  inline bool TryWait() { return 0 == sem_trywait(&sem_); }

  inline void Post() { ASSERT(0 == sem_post(&sem_)); }

  inline int32_t value() {
    int32_t ans = 0;
    ASSERT(0 == sem_getvalue(&sem_, &ans));
    return ans;
  }
};

#endif