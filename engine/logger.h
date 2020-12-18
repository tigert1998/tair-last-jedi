#ifndef TAIR_CONTEST_KV_CONTEST_LOGGER_H_
#define TAIR_CONTEST_KV_CONTEST_LOGGER_H_

#include <cstdarg>
#include <cstdio>

class Logger {
 private:
  FILE* fp_;

 public:
  explicit Logger(FILE* fp);

  void Log(const char* format, ...);
  void LogWithTime(const char* format, ...);
  void Flush();
};

#endif
