/**
 * @file terminal.hpp
 *
 * ターミナルウィンドウを提供する。
 */

#pragma once

#include <deque>
#include <map>
#include "window.hpp"
#include "task.hpp"
#include "layer.hpp"

class Terminal {
 public:
  static const int kRows = 15, kColumns = 60;

  Terminal();
  unsigned int LayerID() const { return layer_id_; }
  Rectangle<int> BlinkCursor();
  void InputKey(uint8_t modifier, uint8_t keycode, char ascii);

  // #@@range_begin(term_fields)
 private:
  std::shared_ptr<ToplevelWindow> window_;
  unsigned int layer_id_;

  Vector2D<int> cursor_{0, 0};
  bool cursor_visible_{false};
  void DrawCursor(bool visible);

  std::array<char, kColumns + 1> linebuf_{};
  void Scroll1();
  // #@@range_end(term_fields)
};

void TaskTerminal(uint64_t task_id, int64_t data);
