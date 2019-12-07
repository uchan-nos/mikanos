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

extern "C" uint64_t SyscallPutChar(char c);
extern "C" uint64_t SyscallPutString(const char* s);

extern "C" int main(int argc, char** argv) {
  stack_ptr = -1;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "+") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a + b);
      SyscallPutChar('+');
    } else if (strcmp(argv[i], "-") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a - b);
      SyscallPutChar('-');
    } else {
      long a = atol(argv[i]);
      Push(a);
      SyscallPutChar('#');
    }
  }
  if (stack_ptr < 0) {
    return 0;
  }
  SyscallPutString("\nhello, this is rpn\n");
  while (1);
  //return static_cast<int>(Pop());
}
