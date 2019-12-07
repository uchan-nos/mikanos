#include <cstring>
#include <cstdlib>
#include <cstdint>

#include "../../kernel/logger.hpp"

int stack_ptr;
long stack[100];

long Pop() {
  long value = stack[stack_ptr];
  --stack_ptr;
  return value;
}

void Push(long value) {
  ++stack_ptr;
  stack[stack_ptr] = value;
}

// #@@range_begin(call_syscall)
extern "C" int64_t SyscallLogString(LogLevel, const char*);

extern "C" int main(int argc, char** argv) {
  stack_ptr = -1;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "+") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a + b);
      SyscallLogString(kWarn, "+");
    } else if (strcmp(argv[i], "-") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a - b);
      SyscallLogString(kWarn, "-");
    } else {
      long a = atol(argv[i]);
      Push(a);
      SyscallLogString(kWarn, "#");
    }
  }
  if (stack_ptr < 0) {
    return 0;
  }
  SyscallLogString(kWarn, "\nhello, this is rpn\n");
  while (1);
  //return static_cast<int>(Pop());
}
// #@@range_end(call_syscall)
