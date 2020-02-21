/**
 * @file task.hpp
 *
 * タスク管理，コンテキスト切り替えのプログラムを集めたファイル。
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <deque>
#include <optional>

#include "error.hpp"
#include "message.hpp"
#include "paging.hpp"

using TaskFunc = void (uint64_t, int64_t);

class TaskManager;

class Task {
 public:
  static const int kDefaultLevel = 1;
  static const size_t kDefaultStackBytes = 4096;

  Task(uint64_t id);
  Task& PushInitialStack(TaskFunc* f, int64_t data);
  uint64_t& StackPointer();
  uint64_t& OSStackPointer();
  uint64_t ID() const;
  Task& Sleep();
  Task& Wakeup();
  void SendMessage(const Message& msg);
  std::optional<Message> ReceiveMessage();

  int Level() const { return level_; }
  bool Running() const { return running_; }

  void SetPML4Page(PageMapEntry* pml4_page) { pml4_page_ = pml4_page; }
  PageMapEntry* PML4Page() const { return pml4_page_; }

 private:
  uint64_t id_;
  std::vector<uint64_t> stack_;
  uint64_t stack_ptr_, os_stack_ptr_;
  std::deque<Message> msgs_;
  unsigned int level_{kDefaultLevel};
  bool running_{false};
  PageMapEntry* pml4_page_{nullptr};

  Task& SetLevel(int level) { level_ = level; return *this; }
  Task& SetRunning(bool running) { running_ = running; return *this; }

  friend TaskManager;
};

class TaskManager {
 public:
  // level: 0 = lowest, kMaxLevel = highest
  static const int kMaxLevel = 3;

  TaskManager();
  Task& NewTask();
  void SwitchTask(bool current_sleep = false);

  void Sleep(Task* task);
  Error Sleep(uint64_t id);
  void Wakeup(Task* task, int level = -1);
  Error Wakeup(uint64_t id, int level = -1);
  Error SendMessage(uint64_t id, const Message& msg);
  Task& CurrentTask();

 private:
  std::vector<std::unique_ptr<Task>> tasks_{};
  uint64_t latest_id_{0};
  std::array<std::deque<Task*>, kMaxLevel + 1> running_{};
  int current_level_{kMaxLevel};
  bool level_changed_{false};

  void ChangeLevelRunning(Task* task, int level);
};

extern TaskManager* task_manager;

void StartTask(uint64_t task_id, int64_t data, TaskFunc* f);
void InitializeTask();
