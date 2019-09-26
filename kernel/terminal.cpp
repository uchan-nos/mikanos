#include "terminal.hpp"

#include <cstring>

#include "font.hpp"
#include "layer.hpp"

// #@@range_begin(term_ctor)
Terminal::Terminal() {
  window_ = std::make_shared<ToplevelWindow>(
      kColumns * 8 + 8 + ToplevelWindow::kMarginX,
      kRows * 16 + 8 + ToplevelWindow::kMarginY,
      screen_config.pixel_format,
      "MikanTerm");
  DrawTerminal(*window_->InnerWriter(), {0, 0}, window_->InnerSize());

  layer_id_ = layer_manager->NewLayer()
    .SetWindow(window_)
    .SetDraggable(true)
    .ID();

  Print(">");
}
// #@@range_end(term_ctor)

Rectangle<int> Terminal::BlinkCursor() {
  cursor_visible_ = !cursor_visible_;
  DrawCursor(cursor_visible_);

  return {CalcCursorPos(), {7, 15}};
}

void Terminal::DrawCursor(bool visible) {
  const auto color = visible ? ToColor(0xffffff) : ToColor(0);
  FillRectangle(*window_->Writer(), CalcCursorPos(), {7, 15}, color);
}

Vector2D<int> Terminal::CalcCursorPos() const {
  return ToplevelWindow::kTopLeftMargin +
      Vector2D<int>{4 + 8 * cursor_.x, 4 + 16 * cursor_.y};
}

Rectangle<int> Terminal::InputKey(
    uint8_t modifier, uint8_t keycode, char ascii) {
  DrawCursor(false);

  Rectangle<int> draw_area{CalcCursorPos(), {8*2, 16}};

  // #@@range_begin(input_key)
  if (ascii == '\n') {
    linebuf_[linebuf_index_] = 0;
    linebuf_index_ = 0;
    cursor_.x = 0;
    if (cursor_.y < kRows - 1) {
      ++cursor_.y;
    } else {
      Scroll1();
    }
    ExecuteLine();
    Print(">");
    draw_area.pos = ToplevelWindow::kTopLeftMargin;
    draw_area.size = window_->InnerSize();
  } else if (ascii == '\b') {
  // #@@range_end(input_key)
    if (cursor_.x > 0) {
      --cursor_.x;
      FillRectangle(*window_->Writer(), CalcCursorPos(), {8, 16}, {0, 0, 0});
      draw_area.pos = CalcCursorPos();

      if (linebuf_index_ > 0) {
        --linebuf_index_;
      }
    }
  } else if (ascii != 0) {
    if (cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
      linebuf_[linebuf_index_] = ascii;
      ++linebuf_index_;
      WriteAscii(*window_->Writer(), CalcCursorPos(), ascii, {255, 255, 255});
      ++cursor_.x;
    }
  }

  DrawCursor(true);

  return draw_area;
}

void Terminal::Scroll1() {
  Rectangle<int> move_src{
    ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4 + 16},
    {8*kColumns, 16*(kRows - 1)}
  };
  window_->Move(ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4}, move_src);
  FillRectangle(*window_->InnerWriter(),
                {4, 4 + 16*cursor_.y}, {8*kColumns, 16}, {0, 0, 0});
}

// #@@range_begin(execute_line)
void Terminal::ExecuteLine() {
  char* command = &linebuf_[0];
  char* first_arg = strchr(&linebuf_[0], ' ');
  if (first_arg) {
    *first_arg = 0;
    ++first_arg;
  }

  if (strcmp(command, "echo") == 0) {
    if (first_arg) {
      Print(first_arg);
    }
    Print("\n");
  } else if (command[0] != 0) {
    Print("no such command: ");
    Print(command);
    Print("\n");
  }
}
// #@@range_end(execute_line)

// #@@range_begin(print)
void Terminal::Print(const char* s) {
  DrawCursor(false);

  auto newline = [this]() {
    cursor_.x = 0;
    if (cursor_.y < kRows - 1) {
      ++cursor_.y;
    } else {
      Scroll1();
    }
  };

  while (*s) {
    if (*s == '\n') {
      newline();
    } else {
      WriteAscii(*window_->Writer(), CalcCursorPos(), *s, {255, 255, 255});
      if (cursor_.x == kColumns - 1) {
        newline();
      } else {
        ++cursor_.x;
      }
    }

    ++s;
  }

  DrawCursor(true);
}
// #@@range_end(print)

void TaskTerminal(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  Terminal* terminal = new Terminal;
  layer_manager->Move(terminal->LayerID(), {100, 200});
  active_layer->Activate(terminal->LayerID());
  layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
  __asm__("sti");

  while (true) {
    __asm__("cli");
    auto msg = task.ReceiveMessage();
    if (!msg) {
      task.Sleep();
      __asm__("sti");
      continue;
    }

    switch (msg->type) {
    case Message::kTimerTimeout:
      {
        const auto area = terminal->BlinkCursor();
        Message msg = MakeLayerMessage(
            task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
        __asm__("cli");
        task_manager->SendMessage(1, msg);
        __asm__("sti");
      }
      break;
    case Message::kKeyPush:
      {
        const auto area = terminal->InputKey(msg->arg.keyboard.modifier,
                                             msg->arg.keyboard.keycode,
                                             msg->arg.keyboard.ascii);
        Message msg = MakeLayerMessage(
            task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
        __asm__("cli");
        task_manager->SendMessage(1, msg);
        __asm__("sti");
      }
      break;
    default:
      break;
    }
  }
}
