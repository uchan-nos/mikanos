#include "../syscall.h"

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(200, 100, 10, 10, "winhello");
  if (err_openwin) {
    SyscallExit(err_openwin);
  }

  SyscallWinWriteString(layer_id, 7, 24, 0xc00000, "hello world!");
  SyscallWinWriteString(layer_id, 24, 40, 0x00c000, "hello world!");
  SyscallWinWriteString(layer_id, 40, 56, 0x0000c0, "hello world!");
  SyscallExit(0);
}
