#include <cstdlib>
#include "../syscall.h"

static constexpr int kWidth = 100, kHeight = 100;

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(kWidth + 8, kHeight + 28, 10, 10, "stars");
  if (err_openwin) {
    exit(err_openwin);
  }

  SyscallWinFillRectangle(layer_id, 4, 24, kWidth, kHeight, 0x000000);

  int num_stars = 100;
  if (argc >= 2) {
    num_stars = atoi(argv[1]);
  }

  for (int i = 0; i < num_stars; ++i) {
    int x = rand() % (kWidth - 2);
    int y = rand() % (kHeight - 2);
    SyscallWinFillRectangle(layer_id, 4 + x, 24 + y, 2, 2, 0xfff100);
  }
  exit(0);
}
