#include "syscall.hpp"

#include <array>
#include <cstdint>
#include <cerrno>

#include "asmfunc.h"
#include "msr.hpp"
#include "logger.hpp"
#include "task.hpp"
#include "terminal.hpp"

// #@@range_begin(syscall_result)
namespace syscall {
  struct Result {
    uint64_t value;
    int error;
  };

#define SYSCALL(name) \
  Result name( \
      uint64_t arg1, uint64_t arg2, uint64_t arg3, \
      uint64_t arg4, uint64_t arg5, uint64_t arg6)
// #@@range_end(syscall_result)

// #@@range_begin(syscall_log_string)
SYSCALL(LogString) {
  if (arg1 != kError && arg1 != kWarn && arg1 != kInfo && arg1 != kDebug) {
    return { 0, EPERM };
  }
  const char* s = reinterpret_cast<const char*>(arg2);
  const auto len = strlen(s);
  if (len > 1024) {
    return { 0, E2BIG };
  }
  Log(static_cast<LogLevel>(arg1), "%s", s);
  return { len, 0 };
}
// #@@range_end(syscall_log_string)

// #@@range_begin(syscall_put_string)
SYSCALL(PutString) {
  const auto fd = arg1;
  const char* s = reinterpret_cast<const char*>(arg2);
  const auto len = arg3;
  if (len > 1024) {
    return { 0, E2BIG };
  }

  if (fd == 1) {
    const auto task_id = task_manager->CurrentTask().ID();
    (*terminals)[task_id]->Print(s, len);
    return { len, 0 };
  }
  return { 0, EBADF };
}
// #@@range_end(syscall_put_string)

#undef SYSCALL

} // namespace syscall

// #@@range_begin(syscall_table)
using SyscallFuncType = syscall::Result (uint64_t, uint64_t, uint64_t,
                                         uint64_t, uint64_t, uint64_t);
extern "C" std::array<SyscallFuncType*, 2> syscall_table{
  /* 0x00 */ syscall::LogString,
  /* 0x01 */ syscall::PutString,
};
// #@@range_end(syscall_table)

void InitializeSyscall() {
  WriteMSR(kIA32_EFER, 0x0501u);
  WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry));
  WriteMSR(kIA32_STAR, static_cast<uint64_t>(8) << 32 |
                       static_cast<uint64_t>(16 | 3) << 48);
  WriteMSR(kIA32_FMASK, 0);
}
