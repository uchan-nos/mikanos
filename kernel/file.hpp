#pragma once

class FileDescriptor {
 public:
  virtual ~FileDescriptor() = default;
  virtual size_t Read(void* buf, size_t len) = 0;
};
