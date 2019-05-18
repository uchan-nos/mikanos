#include "logger.hpp"

#include <cstddef>
#include <cstdio>

namespace {
  LogLevel log_level = kWarn;
}

void SetLogLevel(LogLevel level) {
  log_level = level;
}

int Log(LogLevel level, const char* format, ...) {
  if (level > log_level) {
    return 0;
  }

  va_list ap;
  int result;

  va_start(ap, format);
  result = vprintf(format, ap);
  va_end(ap);

  return result;
}
