#include "logger.hpp"

#include <cstddef>
#include <cstdio>

#include "mikanos-usb/src/lib.rs.h"
#include <stdarg.h>

namespace
{
  LogLevel log_level = kWarn;
}

void SetLogLevel(LogLevel level)
{
  log_level = level;
}

int Log(LogLevel level, const char *format, ...)
{
  if (level > log_level)
  {
    return 0;
  }

  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  rust::put_string(s);
  return result;
}
