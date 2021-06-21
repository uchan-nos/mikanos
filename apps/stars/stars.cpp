#include <cstdlib>
#include <random>
#include "../syscall.h"

static constexpr int kWidth = 100, kHeight = 100;

void WaitEvent() {
  AppEvent events[1];
  while (true) {
    auto [ n, err ] = SyscallReadEvent(events, 1);
    if (err) {
      fprintf(stderr, "ReadEvent failed: %s\n", strerror(err));
      return;
    }
    if (events[0].type == AppEvent::kQuit) {
      return;
    }
  }
}

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(kWidth + 8, kHeight + 28, 10, 10, "stars");
  if (err_openwin) {
    exit(err_openwin);
  }

  SyscallWinFillRectangle(layer_id | LAYER_NO_REDRAW,
                          4, 24, kWidth, kHeight, 0x000000);

  int num_stars = 100;
  if (argc >= 2) {
    num_stars = atoi(argv[1]);
  }

  auto [tick_start, timer_freq] = SyscallGetCurrentTick();

  std::default_random_engine rand_engine;
  std::uniform_int_distribution x_dist(0, kWidth - 2), y_dist(0, kHeight - 2);
  for (int i = 0; i < num_stars; ++i) {
    int x = x_dist(rand_engine);
    int y = y_dist(rand_engine);
    SyscallWinFillRectangle(layer_id | LAYER_NO_REDRAW,
                            4 + x, 24 + y, 2, 2, 0xfff100);
  }
  SyscallWinRedraw(layer_id);

  auto tick_end = SyscallGetCurrentTick();
  printf("%d stars in %lu ms.\n",
         num_stars,
         (tick_end.value - tick_start) * 1000 / timer_freq);

  WaitEvent();
  SyscallCloseWindow(layer_id);

  exit(0);
}
