#include <array>
#include <cmath>
#include <cstdlib>
#include "../syscall.h"

using namespace std;

template <class T>
struct Vector3D {
  T x, y, z;
};

template <class T>
struct Vector2D {
  T x, y;
};

void DrawObj(uint64_t layer_id);
void DrawSurface(uint64_t layer_id, int sur);
bool Sleep(unsigned long ms);

const int kScale = 50, kMargin = 10;
const int kCanvasSize = 3 * kScale + kMargin;
const array<Vector3D<int>, 8> kCube{{
  { 1,  1,  1}, { 1,  1, -1}, { 1, -1,  1}, { 1, -1, -1},
  {-1,  1,  1}, {-1,  1, -1}, {-1, -1,  1}, {-1, -1, -1}
}};
const array<array<int, 4>, 6> kSurface{{
 {0,4,6,2}, {1,3,7,5}, {0,2,3,1}, {0,1,5,4}, {4,5,7,6}, {6,7,3,2}
}};
const array<uint32_t, kSurface.size()> kColor{
  0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff
};

array<Vector3D<double>, kCube.size()> vert;
array<double, kSurface.size()> centerz4;
array<Vector2D<int>, kCube.size()> scr;

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(kCanvasSize + 8, kCanvasSize + 28, 10, 10, "cube");
  if (err_openwin) {
    exit(err_openwin);
  }

  int thx = 0, thy = 0, thz = 0;
  const double to_rad = 3.14159265358979323 / 0x8000;
  for (;;) {
    // 立方体を X, Y, Z 軸回りに回転
    thx = (thx + 182) & 0xffff;
    thy = (thy + 273) & 0xffff;
    thz = (thz + 364) & 0xffff;
    const double xp = cos(thx * to_rad), xa = sin(thx * to_rad);
    const double yp = cos(thy * to_rad), ya = sin(thy * to_rad);
    const double zp = cos(thz * to_rad), za = sin(thz * to_rad);
    for (int i = 0; i < kCube.size(); i++) {
      const auto cv = kCube[i];
      const double zt = kScale*cv.z * xp + kScale*cv.y * xa;
      const double yt = kScale*cv.y * xp - kScale*cv.z * xa;
      const double xt = kScale*cv.x * yp + zt          * ya;
      vert[i].z       = zt          * yp - kScale*cv.x * ya;
      vert[i].x       = xt          * zp - yt          * za;
      vert[i].y       = yt          * zp + xt          * za;
    }

    // 面中心の Z 座標（を 4 倍した値）を 6 面について計算
    for (int sur = 0; sur < kSurface.size(); ++sur) {
      centerz4[sur] = 0;
      for (int i = 0; i < kSurface[sur].size(); ++i) {
        centerz4[sur] += vert[kSurface[sur][i]].z;
      }
    }

    // 画面を一旦クリアし，立方体を描画
    SyscallWinFillRectangle(layer_id | LAYER_NO_REDRAW,
                            4, 24, kCanvasSize, kCanvasSize, 0);
    DrawObj(layer_id | LAYER_NO_REDRAW);
    SyscallWinRedraw(layer_id);
    if (Sleep(50)) {
      break;
    }
  }

  SyscallCloseWindow(layer_id);
  exit(0);
}

void DrawObj(uint64_t layer_id) {
  // オブジェクト座標 vert を スクリーン座標 scr に変換（画面奥が Z+）
  for (int i = 0; i < kCube.size(); i++) {
    const double t = 6*kScale / (vert[i].z + 8*kScale);
    scr[i].x = (vert[i].x * t) + kCanvasSize/2;
    scr[i].y = (vert[i].y * t) + kCanvasSize/2;
  }

  for (;;) {
    // 奥にある（= Z 座標が大きい）オブジェクトから順に描画
    double* const zmax = max_element(centerz4.begin(), centerz4.end());
    if (*zmax == numeric_limits<double>::lowest()) {
      break;
    }
    const int sur = zmax - centerz4.begin();
    centerz4[sur] = numeric_limits<double>::lowest();

    // 法線ベクトルがこっちを向いてる面だけ描画
    const auto v0 = vert[kSurface[sur][0]],
               v1 = vert[kSurface[sur][1]],
               v2 = vert[kSurface[sur][2]];
    const auto e0x = v1.x - v0.x, e0y = v1.y - v0.y, // v0 --> v1
               e1x = v2.x - v1.x, e1y = v2.y - v1.y; // v1 --> v2
    if (e0x * e1y <= e0y * e1x) {
      DrawSurface(layer_id, sur);
    }
  }
}

void DrawSurface(uint64_t layer_id, int sur) {
  const auto& surface = kSurface[sur]; // 描画する面
  int ymin = kCanvasSize, ymax = 0; // 画面の描画範囲 [ymin, ymax]
  int y2x_up[kCanvasSize], y2x_down[kCanvasSize]; // Y, X 座標の組
  for (int i = 0; i < surface.size(); i++) {
    const auto p0 = scr[surface[(i + 3) % 4]], p1 = scr[surface[i]];
    ymin = min(ymin, p1.y);
    ymax = max(ymax, p1.y);
    if (p0.y == p1.y) {
      continue;
    }

    int* y2x;
    int x0, y0, y1, dx;
    if (p0.y < p1.y) { // p0 --> p1 は上る方向
      y2x = y2x_up;
      x0 = p0.x; y0 = p0.y; y1 = p1.y; dx = p1.x - p0.x;
    } else { // p0 --> p1 は下る方向
      y2x = y2x_down;
      x0 = p1.x; y0 = p1.y; y1 = p0.y; dx = p0.x - p1.x;
    }

    const double m = static_cast<double>(dx) / (y1 - y0);
    const auto roundish = dx >= 0 ? static_cast<double(*)(double)>(floor)
                                  : static_cast<double(*)(double)>(ceil);
    for (int y = y0; y <= y1; y++) {
      y2x[y] = roundish(m * (y - y0) + x0);
    }
  }

  for (int y = ymin; y <= ymax; y++) {
    int p0x = min(y2x_up[y], y2x_down[y]);
    int p1x = max(y2x_up[y], y2x_down[y]);
    SyscallWinFillRectangle(layer_id, 4 + p0x, 24 + y, p1x - p0x + 1, 1, kColor[sur]);
  }
}

bool Sleep(unsigned long ms) {
  static unsigned long prev_timeout = 0;
  if (prev_timeout == 0) {
    const auto timeout = SyscallCreateTimer(TIMER_ONESHOT_REL, 1, ms);
    prev_timeout = timeout.value;
  } else {
    prev_timeout += ms;
    SyscallCreateTimer(TIMER_ONESHOT_ABS, 1, prev_timeout);
  }

  AppEvent events[1];
  for (;;) {
    SyscallReadEvent(events, 1);
    if (events[0].type == AppEvent::kTimerTimeout) {
      return false;
    } else if (events[0].type == AppEvent::kQuit) {
      return true;
    }
  }
}
