#include "terminal.hpp"

#include "font.hpp"
#include "layer.hpp"

#include "logger.hpp"

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
}
// #@@range_end(term_ctor)

// #@@range_begin(term_blink)
Rectangle<int> Terminal::BlinkCursor() {
  cursor_visible_ = !cursor_visible_;
  DrawCursor(cursor_visible_);

  return {ToplevelWindow::kTopLeftMargin +
            Vector2D<int>{4 + 8*cursor_.x, 5 + 16*cursor_.y},
          {7, 15}};
}
// #@@range_end(term_blink)

void Terminal::DrawCursor(bool visible) {
  const auto color = visible ? ToColor(0xffffff) : ToColor(0);
  const auto pos = Vector2D<int>{4 + 8*cursor_.x, 5 + 16*cursor_.y};
  FillRectangle(*window_->InnerWriter(), pos, {7, 15}, color);
}

// #@@range_begin(termtask)
void TaskTerminal(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  Terminal* terminal = new Terminal;
  layer_manager->Move(terminal->LayerID(), {100, 200});
  active_layer->Activate(terminal->LayerID());
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
    // #@@range_begin(send_draw_request)
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
    // #@@range_end(send_draw_request)
    default:
      break;
    }
  }
}
// #@@range_end(termtask)
