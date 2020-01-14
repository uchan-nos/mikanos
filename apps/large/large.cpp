#include <cstdlib>
#include "../syscall.h"

char table[3 * 1024 * 1024];

extern "C" void main(int argc, char** argv) {
  SyscallExit(atoi(argv[1]));
}
