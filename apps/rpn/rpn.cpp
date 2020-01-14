#include <cstring>
#include <cstdlib>
#include <cstdio>

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

extern "C" void SyscallExit(int exit_code);

extern "C" void main(int argc, char** argv) {
  stack_ptr = -1;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "+") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a + b);
    } else if (strcmp(argv[i], "-") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a - b);
    } else {
      long a = atol(argv[i]);
      Push(a);
    }
  }
  // #@@range_begin(call_exit)
  long result = 0;
  if (stack_ptr >= 0) {
    result = Pop();
  }

  printf("%ld\n", result);
  SyscallExit(static_cast<int>(result));
  // #@@range_end(call_exit)
}
