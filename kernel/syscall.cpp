#include <array>

#include "logger.hpp"

namespace syscall {

#define SYSCALL(name) \
  uint64_t name( \
      uint64_t arg1, uint64_t arg2, uint64_t arg3, \
      uint64_t arg4, uint64_t arg5, uint64_t arg6)

SYSCALL(PutChar) {
  Log(kWarn, "%c", arg1 & 0xff);
  return 0;
}

SYSCALL(PutString) {
  Log(kWarn, "%s", reinterpret_cast<char*>(arg1));
  return 0;
}

#undef SYSCALL

} // namespace syscall

using SyscallFuncType = uint64_t (uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
extern "C" std::array<SyscallFuncType*, 2> syscall_table{
  /* 0x00 */ syscall::PutChar,
  /* 0x01 */ syscall::PutString,
};
