#include <array>

#include "logger.hpp"

using SyscallFuncType = uint64_t (uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

uint64_t Syscall0(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
  Log(kWarn, "Syscall0(%lu, %lu, %lu, %lu, %lu, %lu)\n",
      arg1, arg2, arg3, arg4, arg5, arg6);
  return 0;
}

uint64_t Syscall2(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
  Log(kWarn, "Syscall2(%lu, %lu, %lu, %lu, %lu, %lu)\n",
      arg1, arg2, arg3, arg4, arg5, arg6);
  return 0;
}

extern "C" std::array<SyscallFuncType*, 3> syscall_table{
  Syscall0,
  nullptr,
  Syscall2,
};
