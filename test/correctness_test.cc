#include <atomic>
#include <cstdio>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <thread>

#include "common/db.h"
#include "engine/config.h"
#include "gtest/gtest.h"
#include "utils.h"

namespace {

class DBTest : public ::testing::Test {
 protected:
  constexpr static int PER_SET = 100;
  constexpr static int PER_GET = 100;

  std::thread ths_[NUM_THREADS];

  const std::string db_file_path_ = "/tmp/correctness";
  DB* db_;

  void SetUp() override {
    FILE* log_file = fopen("/tmp/correctness.log", "w");

    remove(db_file_path_.c_str());

    DB::CreateOrOpen(db_file_path_.c_str(), &db_, log_file);
  }

  void TearDown() override { delete db_; }

 public:
  void Correctness(uint64_t id) {
    std::mt19937 mt(23333 * id * time(nullptr));
    std::uniform_int_distribution<uint32_t> key_dis(
        id * (PER_GET + PER_SET), (id + 1) * (PER_GET + PER_SET) - 1);
    std::uniform_int_distribution<uint32_t> value_len_dis(80, 1024);
    std::uniform_int_distribution<uint32_t> update_value_len_dis(80, 128);
    std::uniform_int_distribution<> choice_dis(0, 1);

    std::map<uint32_t, std::string> map;

    for (int i = 0; i < PER_GET + PER_SET; i++) {
      if (map.size() >= PER_SET || choice_dis(mt) == 0) {
        // read
        uint32_t int_key = key_dis(mt);
        char key[KEY_SIZE];
        memset(key, 0, KEY_SIZE);
        memcpy(key, (char*)&int_key, sizeof(int_key));

        std::string value;
        if (Ok == db_->Get(Slice(key, KEY_SIZE), &value)) {
          EXPECT_TRUE(map.count(int_key) >= 1);
          EXPECT_EQ(value, map[int_key]);
        } else {
          EXPECT_TRUE(map.count(int_key) == 0);
        }
      } else {
        // write
        uint32_t int_key = key_dis(mt);
        uint32_t value_len;
        if (map.count(int_key) >= 1) {
          value_len = update_value_len_dis(mt);
        } else {
          value_len = value_len_dis(mt);
        }

        // construct value: \[[a-z]+\]
        map[int_key] = GenerateRandomString(mt, value_len);
        auto value = map[int_key];

        char key[KEY_SIZE];
        memset(key, 0, KEY_SIZE);
        memcpy(key, (char*)&int_key, sizeof(int_key));

        db_->Set(Slice(key, KEY_SIZE),
                 Slice((char*)value.data(), value.size()));
      }
    }
  }

  void LaunchThreads(const std::function<void(uint64_t)>& func) {
    for (uint32_t i = 0; i < NUM_THREADS; ++i) {
      ths_[i] = std::thread(func, i);
    }

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
      ths_[i].join();
    }
  }
};

TEST_F(DBTest, Correctness) {
  LaunchThreads([this](uint64_t id) -> void { this->Correctness(id); });
}

}  // namespace