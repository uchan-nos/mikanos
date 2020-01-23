#include <cmath>
#include <cstdlib>
#include <random>
#include "../syscall.h"

static constexpr int kRadius = 90;

constexpr uint32_t Color(int deg) {
  if (deg <= 30) {
    return (255 * deg / 30 << 8) | 0xff0000;
  } else if (deg <= 60) {
    return (255 * (60 - deg) / 30) << 16 | 0x00ff00;
  } else if (deg <= 90) {
    return (255 * (deg - 60) / 30) | 0x00ff00;
  } else if (deg <= 120) {
    return (255 * (120 - deg) / 30) << 8 | 0x0000ff;
  } else if (deg <= 150) {
    return (255 * (deg - 120) / 30) << 16 | 0x0000ff;
  } else {
    return (255 * (180 - deg) / 30) | 0xff0000;
  }
};

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(kRadius * 2 + 10 + 8, kRadius + 28, 10, 10, "lines");
  if (err_openwin) {
    exit(err_openwin);
  }

  const int x0 = 4, y0 = 24, x1 = 4 + kRadius + 10, y1 = 24 + kRadius;
  for (int deg = 0; deg <= 90; deg += 5) {
    const int x = kRadius * cos(M_PI * deg / 180.0);
    const int y = kRadius * sin(M_PI * deg / 180.0);
    SyscallWinDrawLine(layer_id, x0, y0, x0 + x, y0 + y, Color(deg));
    SyscallWinDrawLine(layer_id, x1, y1, x1 + x, y1 - y, Color(deg + 90));
  }
  exit(0);
}
