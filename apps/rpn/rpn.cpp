#include <cstring>
#include <cstdlib>
#include <cstdint>

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

extern "C" uint64_t Syscall(uint64_t arg1, uint64_t arg2 = 0, uint64_t arg3 = 0, uint64_t arg4 = 0, uint64_t arg5 = 0);

extern "C" int main(int argc, char** argv) {
  stack_ptr = -1;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "+") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a + b);
      Syscall(1, stack_ptr, a + b);
    } else if (strcmp(argv[i], "-") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a - b);
      Syscall(1, stack_ptr, a - b);
    } else {
      long a = atol(argv[i]);
      Push(a);
      Syscall(1, stack_ptr, a);
    }
  }
  if (stack_ptr < 0) {
    return 0;
  }
  Syscall(2);
  while (1);
  //return static_cast<int>(Pop());
}
