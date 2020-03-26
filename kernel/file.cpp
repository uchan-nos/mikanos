#include "file.hpp"

#include <cstdio>

size_t PrintToFD(FileDescriptor& fd, const char* format, ...) {
  va_list ap;
  int result;
  char s[128];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  fd.Write(s, result);
  return result;
}

