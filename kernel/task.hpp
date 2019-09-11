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

#include "error.hpp"

using TaskFunc = void (uint64_t, int64_t);

class Task {
 public:
  static const size_t kDefaultStackBytes = 4096;

  Task(uint64_t id);
  Task& PushInitialStack(TaskFunc* f, int64_t data);
  uint64_t& StackPointer();
  uint64_t ID() const;
  Task& Sleep();
  Task& Wakeup();

 private:
  uint64_t id_;
  std::vector<uint64_t> stack_;
  uint64_t stack_ptr_;
};

class TaskManager {
 public:
  TaskManager();
  Task& NewTask();
  void SwitchTask(bool current_sleep = false);

  void Sleep(Task* task);
  Error Sleep(uint64_t id);
  void Wakeup(Task* task);
  Error Wakeup(uint64_t id);

 private:
  std::vector<std::unique_ptr<Task>> tasks_{};
  uint64_t latest_id_{0};
  std::deque<Task*> running_{};
};

extern TaskManager* task_manager;

void StartTask(uint64_t task_id, int64_t data, TaskFunc* f);
void InitializeTask();
