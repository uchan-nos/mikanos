#include "syscall.hpp"

#include <array>
#include <cstdint>
#include <cerrno>
#include <cmath>

#include "asmfunc.h"
#include "msr.hpp"
#include "logger.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "font.hpp"
#include "timer.hpp"

namespace syscall {
  struct Result {
    uint64_t value;
    int error;
  };

#define SYSCALL(name) \
  Result name( \
      uint64_t arg1, uint64_t arg2, uint64_t arg3, \
      uint64_t arg4, uint64_t arg5, uint64_t arg6)

SYSCALL(LogString) {
  if (arg1 != kError && arg1 != kWarn && arg1 != kInfo && arg1 != kDebug) {
    return { 0, EPERM };
  }
  const char* s = reinterpret_cast<const char*>(arg2);
  const auto len = strlen(s);
  if (len > 1024) {
    return { 0, E2BIG };
  }
  Log(static_cast<LogLevel>(arg1), "%s", s);
  return { len, 0 };
}

SYSCALL(PutString) {
  const auto fd = arg1;
  const char* s = reinterpret_cast<const char*>(arg2);
  const auto len = arg3;
  if (len > 1024) {
    return { 0, E2BIG };
  }

  if (fd == 1) {
    const auto task_id = task_manager->CurrentTask().ID();
    (*terminals)[task_id]->Print(s, len);
    return { len, 0 };
  }
  return { 0, EBADF };
}

SYSCALL(Exit) {
  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");
  return { task.OSStackPointer(), static_cast<int>(arg1) };
}

SYSCALL(OpenWindow) {
  const int w = arg1, h = arg2, x = arg3, y = arg4;
  const auto title = reinterpret_cast<const char*>(arg5);
  const auto win = std::make_shared<ToplevelWindow>(
      w, h, screen_config.pixel_format, title);

  __asm__("cli");
  const auto layer_id = layer_manager->NewLayer()
    .SetWindow(win)
    .SetDraggable(true)
    .Move({x, y})
    .ID();
  active_layer->Activate(layer_id);
  __asm__("sti");

  return { layer_id, 0 };
}

namespace {
  template <class Func, class... Args>
  Result DoWinFunc(Func f, uint64_t layer_id_flags, Args... args) {
    const uint32_t layer_flags = layer_id_flags >> 32;
    const unsigned int layer_id = layer_id_flags & 0xffffffff;

    __asm__("cli");
    auto layer = layer_manager->FindLayer(layer_id);
    __asm__("sti");
    if (layer == nullptr) {
      return { 0, EBADF };
    }

    const auto res = f(*layer->GetWindow(), args...);
    if (res.error) {
      return res;
    }

    if ((layer_flags & 1) == 0) {
      __asm__("cli");
      layer_manager->Draw(layer_id);
      __asm__("sti");
    }

    return res;
  }
}

SYSCALL(WinWriteString) {
  return DoWinFunc(
      [](Window& win,
         int x, int y, uint32_t color, const char* s) {
        WriteString(*win.Writer(), {x, y}, s, ToColor(color));
        return Result{ 0, 0 };
      }, arg1, arg2, arg3, arg4, reinterpret_cast<const char*>(arg5));
}

SYSCALL(WinFillRectangle) {
  return DoWinFunc(
      [](Window& win,
         int x, int y, int w, int h, uint32_t color) {
        FillRectangle(*win.Writer(), {x, y}, {w, h}, ToColor(color));
        return Result{ 0, 0 };
      }, arg1, arg2, arg3, arg4, arg5, arg6);
}

SYSCALL(GetCurrentTick) {
  return { timer_manager->CurrentTick(), kTimerFreq };
}

SYSCALL(WinRedraw) {
  return DoWinFunc(
      [](Window&) {
        return Result{ 0, 0 };
      }, arg1);
}

// #@@range_begin(win_draw_line)
SYSCALL(WinDrawLine) {
  return DoWinFunc(
      [](Window& win,
         int x0, int y0, int x1, int y1, uint32_t color) {
        auto sign = [](int x) {
          return (x > 0) ? 1 : (x < 0) ? -1 : 0;
        };
        const int dx = x1 - x0 + sign(x1 - x0);
        const int dy = y1 - y0 + sign(y1 - y0);

        if (dx == 0 && dy == 0) {
          win.Writer()->Write({x0, y0}, ToColor(color));
          return Result{ 0, 0 };
        }

        const auto floord = static_cast<double(*)(double)>(floor);
        const auto ceild = static_cast<double(*)(double)>(ceil);

        if (abs(dx) >= abs(dy)) {
          if (dx < 0) {
            std::swap(x0, x1);
            std::swap(y0, y1);
          }
          const auto roundish = y1 >= y0 ? floord : ceild;
          const double m = static_cast<double>(dy) / dx;
          for (int x = x0; x <= x1; ++x) {
            const int y = roundish(m * (x - x0) + y0);
            win.Writer()->Write({x, y}, ToColor(color));
          }
        } else {
          if (dy < 0) {
            std::swap(x0, x1);
            std::swap(y0, y1);
          }
          const auto roundish = x1 >= x0 ? floord : ceild;
          const double m = static_cast<double>(dx) / dy;
          for (int y = y0; y <= y1; ++y) {
            const int x = roundish(m * (y - y0) + x0);
            win.Writer()->Write({x, y}, ToColor(color));
          }
        }
        return Result{ 0, 0 };
      }, arg1, arg2, arg3, arg4, arg5, arg6);
}
// #@@range_end(win_draw_line)

#undef SYSCALL

} // namespace syscall

using SyscallFuncType = syscall::Result (uint64_t, uint64_t, uint64_t,
                                         uint64_t, uint64_t, uint64_t);
extern "C" std::array<SyscallFuncType*, 9> syscall_table{
  /* 0x00 */ syscall::LogString,
  /* 0x01 */ syscall::PutString,
  /* 0x02 */ syscall::Exit,
  /* 0x03 */ syscall::OpenWindow,
  /* 0x04 */ syscall::WinWriteString,
  /* 0x05 */ syscall::WinFillRectangle,
  /* 0x06 */ syscall::GetCurrentTick,
  /* 0x07 */ syscall::WinRedraw,
  /* 0x08 */ syscall::WinDrawLine,
};

void InitializeSyscall() {
  WriteMSR(kIA32_EFER, 0x0501u);
  WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry));
  WriteMSR(kIA32_STAR, static_cast<uint64_t>(8) << 32 |
                       static_cast<uint64_t>(16 | 3) << 48);
  WriteMSR(kIA32_FMASK, 0);
}
