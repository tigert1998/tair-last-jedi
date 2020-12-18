#include <random>

#include "common/db.h"
#include "engine/config.h"
#include "gtest/gtest.h"
#include "utils.h"

TEST(DBTest, Persistence) {
  DB* db;
  std::string db_file_path = "/tmp/persistence";
  remove(db_file_path.c_str());

  std::mt19937 mt(time(nullptr));

  static char key[KEY_SIZE];
  auto gen_key = [&](uint32_t x) {
    memset(key, 0, KEY_SIZE);
    *(uint32_t*)key = x;
  };

  DB::CreateOrOpen(db_file_path.c_str(), &db, nullptr);
  std::map<uint32_t, std::string> dic;
  for (uint32_t i = 0; i < 66; i++) {
    gen_key(i);
    std::string value = GenerateRandomString(mt, 80);
    dic[i] = value;
    db->Set(Slice(key, KEY_SIZE), Slice((char*)value.data(), value.size()));
  }
  delete db;

  DB::CreateOrOpen(db_file_path.c_str(), &db, nullptr);
  for (uint32_t i = 0; i < 10; i++) {
    gen_key(i);
    std::string value = GenerateRandomString(mt, 80);
    dic[i] = value;
    db->Set(Slice(key, KEY_SIZE), Slice((char*)value.data(), value.size()));
  }
  delete db;

  DB::CreateOrOpen(db_file_path.c_str(), &db, nullptr);
  for (uint32_t i = 0; i < 66; i++) {
    gen_key(i);
    std::string ans;
    auto ret = db->Get(Slice(key, KEY_SIZE), &ans);
    EXPECT_EQ(ret, Ok);
    if (ret == Ok) {
      EXPECT_EQ(ans, dic[i]);
    }
  }
}