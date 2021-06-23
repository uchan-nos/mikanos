#include <algorithm>
#include <array>
#include <complex>
#include <cstdio>
#include <cstdlib>

#include "../syscall.h"

struct RGBColor {
  double r, g, b;
};

constexpr std::array<RGBColor, 41> rgb_table{{
  {0.06076 ,0.00000 ,0.11058},  // 波長： 380 nm
  {0.08700 ,0.00000 ,0.16790},  // 390
  {0.13772 ,0.00000 ,0.26354},  // 400
  {0.20707 ,0.00000 ,0.39852},  // 410
  {0.31129 ,0.00000 ,0.60684},  // 420
  {0.39930 ,0.00000 ,0.80505},  // 430
  {0.40542 ,0.00000 ,0.87684},  // 440
  {0.34444 ,0.00000 ,0.88080},  // 450
  {0.11139 ,0.00000 ,0.86037},  // 460
  {0.00000 ,0.15233 ,0.77928},  // 470
  {0.00000 ,0.38550 ,0.65217},  // 480
  {0.00000 ,0.49412 ,0.51919},  // 490
  {0.00000 ,0.59271 ,0.40008},  // 500
  {0.00000 ,0.69549 ,0.25749},  // 510
  {0.00000 ,0.77773 ,0.00000},  // 520
  {0.00000 ,0.81692 ,0.00000},  // 530
  {0.00000 ,0.82625 ,0.00000},  // 540
  {0.00000 ,0.81204 ,0.00000},  // 550
  {0.47369 ,0.77626 ,0.00000},  // 560
  {0.70174 ,0.71523 ,0.00000},  // 570
  {0.84922 ,0.62468 ,0.00000},  // 580
  {0.94726 ,0.49713 ,0.00000},  // 590
  {0.99803 ,0.31072 ,0.00000},  // 600
  {1.00000 ,0.00000 ,0.00000},  // 610
  {0.95520 ,0.00000 ,0.00000},  // 620
  {0.86620 ,0.00000 ,0.00000},  // 630
  {0.76170 ,0.00000 ,0.00000},  // 640
  {0.64495 ,0.00000 ,0.00000},  // 650
  {0.52857 ,0.00000 ,0.00000},  // 660
  {0.41817 ,0.00000 ,0.00000},  // 670
  {0.33202 ,0.00000 ,0.00000},  // 680
  {0.25409 ,0.00000 ,0.00000},  // 690
  {0.19695 ,0.00000 ,0.00000},  // 700
  {0.15326 ,0.00000 ,0.00000},  // 710
  {0.11902 ,0.00000 ,0.00000},  // 720
  {0.09063 ,0.00000 ,0.00000},  // 730
  {0.06898 ,0.00000 ,0.00000},  // 740
  {0.05150 ,0.00000 ,0.00000},  // 750
  {0.04264 ,0.00000 ,0.00000},  // 760
  {0.03666 ,0.00000 ,0.00794},  // 770
  {0.00000 ,0.00000 ,0.00000},  // 780
}};

constexpr int kWaveLenStep = 10, kWaveLenMin = 380, kWaveLenMax = 780;

// 光の波長を RGB 値に変換する関数
// 参考： http://k-hiura.cocolog-nifty.com/blog/2009/06/post-9c5b.html
uint32_t WaveLenToColor(int wlen) {
  auto conv_to_uint32_t = [](RGBColor color) {
    return ((uint32_t)(color.r * 0xff) << 16) |
           ((uint32_t)(color.g * 0xff) << 8) |
            (uint32_t)(color.b * 0xff);
  };

  wlen = std::max(kWaveLenMin, wlen);
  wlen = std::min(kWaveLenMax, wlen);

  // 与えられた周波数が10の倍数の場合、対応するRGB値を rgb_table から直接取ってくる
  const int widx = (wlen - kWaveLenMin) / kWaveLenStep;
  const int wlen_sub = (wlen - kWaveLenMin) - (widx * kWaveLenStep);
  if (wlen_sub == 0) {
    return conv_to_uint32_t(rgb_table[widx]);
  }

  // 周波数が10の倍数でない場合、rgb_table から前後の周波数に対応するRGB値を取得して
  // 線形補間する
  RGBColor ret{
    rgb_table[widx].r,
    rgb_table[widx].g,
    rgb_table[widx].b
  };
  const double rate = (double)wlen_sub / kWaveLenStep;
  ret.r += (rgb_table[widx+1].r - rgb_table[widx].r) * rate;
  ret.g += (rgb_table[widx+1].g - rgb_table[widx].g) * rate;
  ret.b += (rgb_table[widx+1].b - rgb_table[widx].b) * rate;

  return conv_to_uint32_t(ret);
}

int MandelConverge(std::complex<double> z) {
  const auto c = z;
  int n = 0;
  const int nmax = 100;

  for (; n < nmax && std::norm(z) <= 4; ++n) {
    z = z*z + c;
  }

  return n;
}

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

constexpr int kWidth = 78*4, kHeight = 52*4;

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(kWidth + 8, kHeight + 28, 10, 10, "mandel");
  if (err_openwin) {
    exit(err_openwin);
  }

  // xmin: 実部の最小値, ymin: 虚部の最小値
  const double xmin  = -2.3,   ymin  = -1.3;
  const double xstep = 0.0125, ystep = 0.0125;

  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      // 漸化式の計算が収束するまでの再帰回数 depth (100 を上限とする) を得る
      int depth = MandelConverge({xmin + xstep*x, ymin + ystep*y});

      SyscallWinFillRectangle(
        layer_id | LAYER_NO_REDRAW,
        4+x, 24+y, 1, 1,
        WaveLenToColor(depth * 4 + kWaveLenMin)
      );
    }
  }
  SyscallWinRedraw(layer_id);

  WaitEvent();
  SyscallCloseWindow(layer_id);

  exit(0);
}
