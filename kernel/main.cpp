/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル．
 */

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <deque>
#include <limits>
#include <numeric>
#include <vector>

#include "frame_buffer_config.hpp"
#include "memory_map.hpp"
#include "graphics.hpp"
#include "mouse.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "usb/xhci/xhci.hpp"
#include "interrupt.hpp"
#include "asmfunc.h"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "message.hpp"
#include "timer.hpp"
#include "acpi.hpp"
#include "keyboard.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "fat.hpp"
#include "syscall.hpp"
#include "uefi.hpp"

__attribute__((format(printf, 1, 2))) int printk(const char* format, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  console->PutString(s);
  return result;
}

std::shared_ptr<ToplevelWindow> main_window;
unsigned int main_window_layer_id;
void InitializeMainWindow() {
  main_window = std::make_shared<ToplevelWindow>(
      160, 52, screen_config.pixel_format, "Hello Window");

  main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .SetDraggable(true)
    .Move({300, 100})
    .ID();

  layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());
}

std::shared_ptr<ToplevelWindow> text_window;
unsigned int text_window_layer_id;
void InitializeTextWindow() {
  const int win_w = 160;
  const int win_h = 52;

  text_window = std::make_shared<ToplevelWindow>(
      win_w, win_h, screen_config.pixel_format, "Text Box Test");
  DrawTextbox(*text_window->InnerWriter(), {0, 0}, text_window->InnerSize());

  text_window_layer_id = layer_manager->NewLayer()
    .SetWindow(text_window)
    .SetDraggable(true)
    .Move({500, 100})
    .ID();

  layer_manager->UpDown(text_window_layer_id, std::numeric_limits<int>::max());
}

int text_window_index;

void DrawTextCursor(bool visible) {
  const auto color = visible ? ToColor(0) : ToColor(0xffffff);
  const auto pos = Vector2D<int>{4 + 8*text_window_index, 5};
  FillRectangle(*text_window->InnerWriter(), pos, {7, 15}, color);
}

void InputTextWindow(char c) {
  if (c == 0) {
    return;
  }

  auto pos = []() { return Vector2D<int>{4 + 8*text_window_index, 6}; };

  const int max_chars = (text_window->InnerSize().x - 8) / 8 - 1;
  if (c == '\b' && text_window_index > 0) {
    DrawTextCursor(false);
    --text_window_index;
    FillRectangle(*text_window->InnerWriter(), pos(), {8, 16}, ToColor(0xffffff));
    DrawTextCursor(true);
  } else if (c >= ' ' && text_window_index < max_chars) {
    DrawTextCursor(false);
    WriteAscii(*text_window->InnerWriter(), pos(), c, ToColor(0));
    ++text_window_index;
    DrawTextCursor(true);
  }

  layer_manager->Draw(text_window_layer_id);
}

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

// デスクトップの右下（タスクバーの右端）に現在時刻を表示する
void TaskWallclock(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  auto clock_window = std::make_shared<Window>(
      8 * 10, 16 * 2, screen_config.pixel_format);
  const auto clock_window_layer_id = layer_manager->NewLayer()
    .SetWindow(clock_window)
    .SetDraggable(false)
    .Move(ScreenSize() - clock_window->Size() - Vector2D<int>{4, 8})
    .ID();
  layer_manager->UpDown(clock_window_layer_id, 2);
  __asm__("sti");

  auto draw_current_time = [&]() {
    EFI_TIME t;
    uefi_rt->GetTime(&t, nullptr);

    FillRectangle(*clock_window->Writer(),
                  {0, 0}, clock_window->Size(), {0, 0, 0});

    char s[64];
    sprintf(s, "%04d-%02d-%02d", t.Year, t.Month, t.Day);
    WriteString(*clock_window->Writer(), {0, 0}, s, {255, 255, 255});
    sprintf(s, "%02d:%02d:%02d", t.Hour, t.Minute, t.Second);
    WriteString(*clock_window->Writer(), {0, 16}, s, {255, 255, 255});

    Message msg{Message::kLayer, task_id};
    msg.arg.layer.layer_id = clock_window_layer_id;
    msg.arg.layer.op = LayerOperation::Draw;

    __asm__("cli");
    task_manager->SendMessage(1, msg);
    __asm__("sti");
  };

  draw_current_time();
  timer_manager->AddTimer(
      Timer{timer_manager->CurrentTick(), 1, task_id});

  while (true) {
    __asm__("cli");
    auto msg = task.ReceiveMessage();
    if (!msg) {
      task.Sleep();
      __asm__("sti");
      continue;
    }
    __asm__("sti");

    if (msg->type == Message::kTimerTimeout) {
      draw_current_time();
      timer_manager->AddTimer(
          Timer{msg->arg.timer.timeout + kTimerFreq, 1, task_id});
    }
  }
}

extern "C" void KernelMainNewStack(
    const FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref,
    const acpi::RSDP& acpi_table,
    void* volume_image,
    EFI_RUNTIME_SERVICES* rt) {
  MemoryMap memory_map{memory_map_ref};
  uefi_rt = rt;

  InitializeGraphics(frame_buffer_config_ref);
  InitializeConsole();

  printk("Welcome to MikanOS!\n");
  SetLogLevel(kWarn);

  InitializeSegmentation();
  InitializePaging();
  InitializeMemoryManager(memory_map);
  InitializeTSS();
  InitializeInterrupt();

  fat::Initialize(volume_image);
  InitializeFont();
  InitializePCI();

  InitializeLayer();
  InitializeMainWindow();
  InitializeTextWindow();
  layer_manager->Draw({{0, 0}, ScreenSize()});

  acpi::Initialize(acpi_table);
  InitializeLAPICTimer();

  const int kTextboxCursorTimer = 1;
  const int kTimer05Sec = static_cast<int>(kTimerFreq * 0.5);
  timer_manager->AddTimer(Timer{kTimer05Sec, kTextboxCursorTimer, 1});
  bool textbox_cursor_visible = false;

  InitializeSyscall();

  InitializeTask();
  Task& main_task = task_manager->CurrentTask();

  usb::xhci::Initialize();
  InitializeKeyboard();
  InitializeMouse();

  app_loads = new std::map<fat::DirectoryEntry*, AppLoadInfo>;
  task_manager->NewTask()
    .InitContext(TaskTerminal, 0)
    .Wakeup();

  task_manager->NewTask()
    .InitContext(TaskWallclock, 0)
    .Wakeup();

  const int kMouseKeyTimer = 2;
  const int kMouseKeyTimerFirstTimeout = static_cast<int>(kTimerFreq * 0.25);
  const int kMouseKeyTimerInterval = static_cast<int>(kTimerFreq * 0.025);
  const int kMouseKeyDelta = 5;
  uint8_t mouse_key_buttons = 0;
  bool mouse_key_left = false, mouse_key_right = false;
  bool mouse_key_up = false, mouse_key_down = false;
  bool mouse_key_timer_running = false;

  char str[128];

  while (true) {
    __asm__("cli");
    const auto tick = timer_manager->CurrentTick();
    __asm__("sti");

    sprintf(str, "%010lu", tick);
    FillRectangle(*main_window->InnerWriter(), {20, 4}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
    WriteString(*main_window->InnerWriter(), {20, 4}, str, {0, 0, 0});
    layer_manager->Draw(main_window_layer_id);

    __asm__("cli");
    auto msg = main_task.ReceiveMessage();
    if (!msg) {
      main_task.Sleep();
      __asm__("sti");
      continue;
    }

    __asm__("sti");

    switch (msg->type) {
    case Message::kInterruptXHCI:
      usb::xhci::ProcessEvents();
      break;
    case Message::kTimerTimeout:
      if (msg->arg.timer.value == kTextboxCursorTimer) {
        __asm__("cli");
        timer_manager->AddTimer(
            Timer{msg->arg.timer.timeout + kTimer05Sec, kTextboxCursorTimer, 1});
        __asm__("sti");
        textbox_cursor_visible = !textbox_cursor_visible;
        DrawTextCursor(textbox_cursor_visible);
        layer_manager->Draw(text_window_layer_id);
      } else if (msg->arg.timer.value == kMouseKeyTimer) {
        if (mouse_key_left || mouse_key_right || mouse_key_up || mouse_key_down) {
          int8_t displacement_x = 0, displacement_y = 0;
          if (mouse_key_left) displacement_x -= kMouseKeyDelta;
          if (mouse_key_right) displacement_x += kMouseKeyDelta;
          if (mouse_key_up) displacement_y -= kMouseKeyDelta;
          if (mouse_key_down) displacement_y += kMouseKeyDelta;
          __asm__("cli");
          mouse->OnInterrupt(mouse_key_buttons, displacement_x, displacement_y);
          timer_manager->AddTimer(
              Timer{msg->arg.timer.timeout + kMouseKeyTimerInterval, kMouseKeyTimer, 1});
          __asm__("sti");
        } else {
          mouse_key_timer_running = false;
        }
      }
      break;
    case Message::kKeyPush:
      if ((msg->arg.keyboard.modifier & 5) == 5 /* Ctrl+Alt */) {
        bool is_mouse_key_operation = false;
        uint8_t prev_buttons = mouse_key_buttons;
        if (msg->arg.keyboard.keycode == 40 /* Enter */) {
          if (msg->arg.keyboard.press) {
            mouse_key_buttons |= 1;
          } else {
            mouse_key_buttons &= ~1;
          }
          is_mouse_key_operation = true;
        } else if (msg->arg.keyboard.keycode == 42 /* Backspace */) {
          if (msg->arg.keyboard.press) {
            mouse_key_buttons |= 2;
          } else {
            mouse_key_buttons &= ~2;
          }
          is_mouse_key_operation = true;
        } else if (msg->arg.keyboard.keycode == 80 /* ← */) {
          mouse_key_left = !!msg->arg.keyboard.press;
          is_mouse_key_operation = true;
        } else if (msg->arg.keyboard.keycode == 79 /* → */) {
          mouse_key_right = !!msg->arg.keyboard.press;
          is_mouse_key_operation = true;
        } else if (msg->arg.keyboard.keycode == 82 /* ↑ */) {
          mouse_key_up = !!msg->arg.keyboard.press;
          is_mouse_key_operation = true;
        } else if (msg->arg.keyboard.keycode == 81 /* ↓ */) {
          mouse_key_down = !!msg->arg.keyboard.press;
          is_mouse_key_operation = true;
        }
        int8_t displacement_x = 0, displacement_y = 0;
        if (mouse_key_left) displacement_x -= kMouseKeyDelta;
        if (mouse_key_right) displacement_x += kMouseKeyDelta;
        if (mouse_key_up) displacement_y -= kMouseKeyDelta;
        if (mouse_key_down) displacement_y += kMouseKeyDelta;
        if (mouse_key_buttons != prev_buttons ||
            displacement_x != 0 || displacement_y != 0) {
          __asm__("cli");
          mouse->OnInterrupt(mouse_key_buttons, displacement_x, displacement_y);
          __asm__("sti");
        }
        if (!mouse_key_timer_running &&
            (mouse_key_left || mouse_key_right || mouse_key_up || mouse_key_down)) {
          __asm__("cli");
          timer_manager->AddTimer(
              Timer{tick + kMouseKeyTimerFirstTimeout, kMouseKeyTimer, 1});
          __asm__("sti");
          mouse_key_timer_running = true;
        }
        if (is_mouse_key_operation) break;
      } else {
        if (mouse_key_buttons != 0) {
          __asm__("cli");
          mouse->OnInterrupt(0, 0, 0);
          __asm__("sti");
        }
        mouse_key_buttons = 0;
        mouse_key_left = false;
        mouse_key_right = false;
        mouse_key_up = false;
        mouse_key_down = false;
      }
      if (auto act = active_layer->GetActive(); act == text_window_layer_id) {
        if (msg->arg.keyboard.press) {
          InputTextWindow(msg->arg.keyboard.ascii);
        }
      } else if (msg->arg.keyboard.press &&
                 msg->arg.keyboard.keycode == 59 /* F2 */) {
        task_manager->NewTask()
          .InitContext(TaskTerminal, 0)
          .Wakeup();
      } else {
        __asm__("cli");
        auto task_it = layer_task_map->find(act);
        __asm__("sti");
        if (task_it != layer_task_map->end()) {
          __asm__("cli");
          task_manager->SendMessage(task_it->second, *msg);
          __asm__("sti");
        } else {
          printk("key push not handled: keycode %02x, ascii %02x\n",
              msg->arg.keyboard.keycode,
              msg->arg.keyboard.ascii);
        }
      }
      break;
    case Message::kLayer:
      ProcessLayerMessage(*msg);
      __asm__("cli");
      task_manager->SendMessage(msg->src_task, Message{Message::kLayerFinish});
      __asm__("sti");
      break;
    default:
      Log(kError, "Unknown message type: %d\n", msg->type);
    }
  }
}

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}
