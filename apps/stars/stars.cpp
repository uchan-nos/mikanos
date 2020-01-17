#include <cstdlib>
#include "../syscall.h"

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(100, 100, 10, 10, "stars");
  if (err_openwin) {
    exit(err_openwin);
  }

  SyscallWinFillRectangle(layer_id, 2, 10, 10, 20, 0xcc00cc);
  exit(0);
}
