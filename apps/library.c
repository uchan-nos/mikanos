#include "syscall.h"

extern int main(int, char**);

void _start(int argc, char** argv) {
  SyscallExit(main(argc, argv));
}
