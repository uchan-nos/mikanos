/**
 * @file terminal.hpp
 *
 * ターミナルウィンドウを提供する。
 */

#pragma once

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include "window.hpp"
#include "task.hpp"
#include "layer.hpp"
#include "fat.hpp"

struct AppLoadInfo {
  uint64_t vaddr_end, entry;
  PageMapEntry* pml4;
};

extern std::map<fat::DirectoryEntry*, AppLoadInfo>* app_loads;

struct TerminalDescriptor {
  std::string command_line;
  bool exit_after_command;
  bool show_window;
  std::array<std::shared_ptr<FileDescriptor>, 3> files;
};

class Terminal {
 public:
  static const int kRows = 15, kColumns = 60;
  static const int kLineMax = 128;

  Terminal(Task& task, const TerminalDescriptor* term_desc);
  unsigned int LayerID() const { return layer_id_; }
  Rectangle<int> BlinkCursor();
  Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);

  void Print(const char* s, std::optional<size_t> len = std::nullopt);

  Task& UnderlyingTask() const { return task_; }
  int LastExitCode() const { return last_exit_code_; }
  void Redraw();

 private:
  std::shared_ptr<ToplevelWindow> window_;
  unsigned int layer_id_;
  Task& task_;

  Vector2D<int> cursor_{0, 0};
  bool cursor_visible_{false};
  void DrawCursor(bool visible);
  Vector2D<int> CalcCursorPos() const;

  int linebuf_index_{0};
  std::array<char, kLineMax> linebuf_{};
  void Scroll1();

  void ExecuteLine();
  WithError<int> ExecuteFile(fat::DirectoryEntry& file_entry,
                             char* command, char* first_arg);
  void Print(char32_t c);

  std::deque<std::array<char, kLineMax>> cmd_history_{};
  int cmd_history_index_{-1};
  Rectangle<int> HistoryUpDown(int direction);

  bool show_window_;
  std::array<std::shared_ptr<FileDescriptor>, 3> files_;
  int last_exit_code_{0};
};

void TaskTerminal(uint64_t task_id, int64_t data);

class TerminalFileDescriptor : public FileDescriptor {
 public:
  explicit TerminalFileDescriptor(Terminal& term);
  size_t Read(void* buf, size_t len) override;
  size_t Write(const void* buf, size_t len) override;
  size_t Size() const override { return 0; }
  size_t Load(void* buf, size_t len, size_t offset) override;

 private:
  Terminal& term_;
};

class PipeDescriptor : public FileDescriptor {
 public:
  explicit PipeDescriptor(Task& task);
  size_t Read(void* buf, size_t len) override;
  size_t Write(const void* buf, size_t len) override;
  size_t Size() const override { return 0; }
  size_t Load(void* buf, size_t len, size_t offset) override { return 0; }

  void FinishWrite();

 private:
  Task& task_;
  char data_[16];
  size_t len_{0};
  bool closed_{false};
};
