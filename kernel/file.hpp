#pragma once

#include "error.hpp"

class FileDescriptor {
 public:
  virtual ~FileDescriptor() = default;
  virtual size_t Read(void* buf, size_t len) = 0;
  virtual size_t Write(const void* buf, size_t len) = 0;
  virtual size_t Size() const = 0;
  virtual Error Load(void* buf, size_t offset, size_t len) = 0;
};
