/**
 * @file task.hpp
 *
 * タスク管理，コンテキスト切り替えのプログラムを集めたファイル。
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

using TaskFunc = void (uint64_t, int64_t);

class Task {
 public:
  static const size_t kDefaultStackBytes = 4096;

  Task(uint64_t id);
  Task& PushInitialStack(TaskFunc* f, int64_t data);
  uint64_t& StackPointer();

 private:
  uint64_t id_;
  std::vector<uint64_t> stack_;
  uint64_t stack_ptr_;
};

class TaskManager {
 public:
  TaskManager();
  Task& NewTask();
  void SwitchTask();

 private:
  std::vector<std::unique_ptr<Task>> tasks_{};
  uint64_t latest_id_{0};
  size_t current_task_index_{0};
};

extern TaskManager* task_manager;

void StartTask(uint64_t task_id, int64_t data, TaskFunc* f);
void InitializeTask();
