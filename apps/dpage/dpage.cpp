#include <cstdio>
#include <cstdlib>
#include "../syscall.h"

extern "C" void main(int argc, char** argv) {
  char s[64];

  SyscallResult res = SyscallDemandPages(1, 0);
  int* p = reinterpret_cast<int*>(res.value);

  sprintf(s, "finished to allocate pages: %p\n", p);
  SyscallLogString(kWarn, s);

  for (int i = 0; i < 4096 / sizeof(int); ++i) {
    p[i] = i;
  }

  SyscallLogString(kWarn, "finished to write\n");

  exit(0);
}
