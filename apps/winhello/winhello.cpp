#include "../syscall.h"

extern "C" void main(int argc, char** argv) {
  SyscallOpenWindow(200, 100, 10, 10, "winhello");
  SyscallExit(0);
}
