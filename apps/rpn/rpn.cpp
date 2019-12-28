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

void FormatLong(char* s, long v) {
  if (v == 0) {
    s[0] = '0';
    s[1] = '\n';
    s[2] = '\0';
    return;
  }

  int i = 0;
  while (v) {
    s[i] = (v % 10) + '0';
    v /= 10;
    ++i;
  }
  s[i] = '\n';
  s[i + 1] = '\0';
  for (int j = 0; j < i/2; ++j) {
    char tmp = s[j];
    s[j] = s[i - j - 1];
    s[i - j - 1] = tmp;
  }
}

extern "C" int64_t SyscallLogString(LogLevel, const char*);
extern "C" int64_t SyscallPutString(const char*);

extern "C" int main(int argc, char** argv) {
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
  long result = 0;
  if (stack_ptr >= 0) {
    result = Pop();
  }

  char s[64];
  FormatLong(s, result);
  SyscallPutString(s);
  while (1);
  //return static_cast<int>(Pop());
}
