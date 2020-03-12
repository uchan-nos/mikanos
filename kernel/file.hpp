#pragma once

#include "error.hpp"

// #@@range_begin(file_fd)
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
// #@@range_end(file_fd)
