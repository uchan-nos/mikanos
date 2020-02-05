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
#include "keyboard.hpp"
#include "app_event.hpp"

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

  const auto task_id = task_manager->CurrentTask().ID();
  layer_task_map->insert(std::make_pair(layer_id, task_id));
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

SYSCALL(CloseWindow) {
  const unsigned int layer_id = arg1 & 0xffffffff;
  const auto layer = layer_manager->FindLayer(layer_id);

  if (layer == nullptr) {
    return { EBADF, 0 };
  }

  const auto layer_pos = layer->GetPosition();
  const auto win_size = layer->GetWindow()->Size();

  __asm__("cli");
  active_layer->Activate(0);
  layer_manager->RemoveLayer(layer_id);
  layer_manager->Draw({layer_pos, win_size});
  layer_task_map->erase(layer_id);
  __asm__("sti");

  return { 0, 0 };
}

SYSCALL(ReadEvent) {
  if (arg1 < 0x8000'0000'0000'0000) {
    return { 0, EFAULT };
  }
  const auto app_events = reinterpret_cast<AppEvent*>(arg1);
  const size_t len = arg2;

  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");
  size_t i = 0;

  while (i < len) {
    __asm__("cli");
    auto msg = task.ReceiveMessage();
    if (!msg && i == 0) {
      task.Sleep();
      continue;
    }
    __asm__("sti");

    if (!msg) {
      break;
    }

    switch (msg->type) {
    case Message::kKeyPush:
      if (msg->arg.keyboard.keycode == 20 /* Q key */ &&
          msg->arg.keyboard.modifier & (kLControlBitMask | kRControlBitMask)) {
        app_events[i].type = AppEvent::kQuit;
        ++i;
      }
      break;
    case Message::kMouseMove:
      app_events[i].type = AppEvent::kMouseMove;
      app_events[i].arg.mouse_move.x = msg->arg.mouse_move.x;
      app_events[i].arg.mouse_move.y = msg->arg.mouse_move.y;
      app_events[i].arg.mouse_move.dx = msg->arg.mouse_move.dx;
      app_events[i].arg.mouse_move.dy = msg->arg.mouse_move.dy;
      app_events[i].arg.mouse_move.buttons = msg->arg.mouse_move.buttons;
      ++i;
      break;
    case Message::kMouseButton:
      app_events[i].type = AppEvent::kMouseButton;
      app_events[i].arg.mouse_button.x = msg->arg.mouse_button.x;
      app_events[i].arg.mouse_button.y = msg->arg.mouse_button.y;
      app_events[i].arg.mouse_button.press = msg->arg.mouse_button.press;
      app_events[i].arg.mouse_button.button = msg->arg.mouse_button.button;
      ++i;
      break;
    // #@@range_begin(handle_timeout)
    case Message::kTimerTimeout:
      if (msg->arg.timer.value < 0) {
        app_events[i].type = AppEvent::kTimerTimeout;
        app_events[i].arg.timer.timeout = msg->arg.timer.timeout;
        app_events[i].arg.timer.value = -msg->arg.timer.value;
        ++i;
      }
      break;
    // #@@range_end(handle_timeout)
    default:
      Log(kInfo, "uncaught event type: %u\n", msg->type);
    }
  }

  return { i, 0 };
}

// #@@range_begin(create_timer)
SYSCALL(CreateTimer) {
  const unsigned int mode = arg1;
  const int timer_value = arg2;
  if (timer_value <= 0) {
    return { 0, EINVAL };
  }

  __asm__("cli");
  const uint64_t task_id = task_manager->CurrentTask().ID();
  __asm__("sti");

  unsigned long timeout = arg3 * kTimerFreq / 1000;
  if (mode & 1) { // relative
    timeout += timer_manager->CurrentTick();
  }

  __asm__("cli");
  timer_manager->AddTimer(Timer{timeout, -timer_value, task_id});
  __asm__("sti");
  return { timeout * 1000 / kTimerFreq, 0 };
}
// #@@range_end(create_timer)

#undef SYSCALL

} // namespace syscall

using SyscallFuncType = syscall::Result (uint64_t, uint64_t, uint64_t,
                                         uint64_t, uint64_t, uint64_t);
extern "C" std::array<SyscallFuncType*, 0xc> syscall_table{
  /* 0x00 */ syscall::LogString,
  /* 0x01 */ syscall::PutString,
  /* 0x02 */ syscall::Exit,
  /* 0x03 */ syscall::OpenWindow,
  /* 0x04 */ syscall::WinWriteString,
  /* 0x05 */ syscall::WinFillRectangle,
  /* 0x06 */ syscall::GetCurrentTick,
  /* 0x07 */ syscall::WinRedraw,
  /* 0x08 */ syscall::WinDrawLine,
  /* 0x09 */ syscall::CloseWindow,
  /* 0x0a */ syscall::ReadEvent,
  /* 0x0b */ syscall::CreateTimer,
};

void InitializeSyscall() {
  WriteMSR(kIA32_EFER, 0x0501u);
  WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry));
  WriteMSR(kIA32_STAR, static_cast<uint64_t>(8) << 32 |
                       static_cast<uint64_t>(16 | 3) << 48);
  WriteMSR(kIA32_FMASK, 0);
}
