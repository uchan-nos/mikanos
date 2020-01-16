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
#include "fat.hpp"

class Terminal {
 public:
  static const int kRows = 15, kColumns = 60;
  static const int kLineMax = 128;

  Terminal(uint64_t task_id);
  unsigned int LayerID() const { return layer_id_; }
  Rectangle<int> BlinkCursor();
  Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);

  void Print(const char* s);

 private:
  std::shared_ptr<ToplevelWindow> window_;
  unsigned int layer_id_;
  uint64_t task_id_;

  Vector2D<int> cursor_{0, 0};
  bool cursor_visible_{false};
  void DrawCursor(bool visible);
  Vector2D<int> CalcCursorPos() const;

  int linebuf_index_{0};
  std::array<char, kLineMax> linebuf_{};
  void Scroll1();

  void ExecuteLine();
  Error ExecuteFile(const fat::DirectoryEntry& file_entry, char* command, char* first_arg);
  void Print(char c);

  std::deque<std::array<char, kLineMax>> cmd_history_{};
  int cmd_history_index_{-1};
  Rectangle<int> HistoryUpDown(int direction);
};

extern std::map<uint64_t, Terminal*>* terminals;
void TaskTerminal(uint64_t task_id, int64_t data);
