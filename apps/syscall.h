#include <cstddef>
#include <cstdint>
#include "../kernel/logger.hpp"

extern "C" {

struct SyscallResult {
  uint64_t value;
  int error;
};

SyscallResult SyscallLogString(LogLevel level, const char* message);
SyscallResult SyscallPutString(int fd, const char* s, size_t len);
void SyscallExit(int exit_code);
SyscallResult SyscallOpenWindow(int w, int h, int x, int y, const char* title);
SyscallResult SyscallWinWriteString(unsigned int layer_id, int x, int y, uint32_t color, const char* s);

} // extern "C"
