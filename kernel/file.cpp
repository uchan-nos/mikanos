#include "file.hpp"

#include <cstdio>
#include <vector>

size_t PrintToFD(FileDescriptor& fd, const char* format, ...) {
  constexpr int BUFFER_SIZE = 128;
  va_list ap;
  int result;
  char s[BUFFER_SIZE];

  va_start(ap, format);
  result = vsnprintf(s, BUFFER_SIZE, format, ap);
  va_end(ap);
  if (result < BUFFER_SIZE) {
    fd.Write(s, result);
    return result;
  }

  std::vector<char> s2(result + 1);
  va_start(ap, format);
  vsnprintf(&s2[0], result + 1, format, ap);
  va_end(ap);
  fd.Write(&s2[0], result);
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
