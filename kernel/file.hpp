#pragma once

#include <cstddef>
#include "error.hpp"

class FileDescriptor {
 public:
  virtual ~FileDescriptor() = default;
  virtual size_t Read(void* buf, size_t len) = 0;
  virtual size_t Write(const void* buf, size_t len) = 0;
  virtual size_t Size() const = 0;

  /** @brief Load reads file content without changing internal offset
   */
  virtual size_t Load(void* buf, size_t len, size_t offset) = 0;
};

size_t PrintToFD(FileDescriptor& fd, const char* format, ...)
    __attribute__((format(printf, 2, 3)));
size_t ReadDelim(FileDescriptor& fd, char delim, char* buf, size_t len);
