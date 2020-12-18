#include "logger.h"

#include <cstring>
#include <ctime>

Logger::Logger(FILE *fp) : fp_(fp) {}

void Logger::Log(const char *format, ...) {
  if (fp_ == nullptr) return;
  char new_fmt[512];
  strcpy(new_fmt, "[unknown timestamp] ");
  strcat(new_fmt, format);
  strcat(new_fmt, "\n");

  va_list argptr;
  va_start(argptr, format);
  vfprintf(fp_, new_fmt, argptr);
  va_end(argptr);
}

void Logger::LogWithTime(const char *format, ...) {
  if (fp_ == nullptr) return;
  char new_fmt[512];
  time_t now = time(0);
  tm *t = gmtime(&now);
  strftime(new_fmt, sizeof(new_fmt), "[%H:%M:%S] ", t);
  strcat(new_fmt, format);
  strcat(new_fmt, "\n");

  va_list argptr;
  va_start(argptr, format);
  vfprintf(fp_, new_fmt, argptr);
  va_end(argptr);
}

void Logger::Flush() {
  if (fp_ == nullptr) return;
  fflush(fp_);
}