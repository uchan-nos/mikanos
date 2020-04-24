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

size_t ReadDelim(FileDescriptor& fd, char delim, char* buf, size_t len) {
  size_t i = 0;
  for (; i < len - 1; ++i) {
    if (fd.Read(&buf[i], 1) == 0) {
      break;
    }
    if (buf[i] == delim) {
      ++i;
      break;
    }
  }
  buf[i] = '\0';
  return i;
}
