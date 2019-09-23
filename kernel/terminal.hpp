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

// #@@range_begin(term)
class Terminal {
 public:
  static const int kRows = 15, kColumns = 60;

  Terminal();
  unsigned int LayerID() const { return layer_id_; }
  Rectangle<int> BlinkCursor();

 private:
  std::shared_ptr<ToplevelWindow> window_;
  unsigned int layer_id_;

  Vector2D<int> cursor_{0, 0};
  bool cursor_visible_{false};
  void DrawCursor(bool visible);
};

void TaskTerminal(uint64_t task_id, int64_t data);
// #@@range_end(term)
