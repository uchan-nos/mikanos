#include "task.hpp"

#include "asmfunc.h"
#include "timer.hpp"

Task::Task(uint64_t id) : id_{id} {
  const size_t stack_size = kDefaultStackBytes / sizeof(stack_[0]);
  stack_.resize(stack_size);
  stack_ptr_ = reinterpret_cast<uint64_t>(&stack_[stack_size]);

  if ((stack_ptr_ & 0xf) != 0) {
    stack_ptr_ -= 8;
  }
}

Task& Task::PushInitialStack(TaskFunc* f, int64_t data) {
  auto push = [this](uint64_t value) {
    stack_ptr_ -= 8;
    *reinterpret_cast<uint64_t*>(stack_ptr_) = value;
  };

  if ((stack_ptr_ & 0xf) == 0) {
    push(0); // not-used
  }

  push(reinterpret_cast<uint64_t>(StartTask));
  push(0); // rax
  push(0); // rbx
  push(0); // rcx
  push(reinterpret_cast<uint64_t>(f)); // rdx
  push(id_); // rdi
  push(data); // rsi
  push(0); // rbp
  push(0); // r8
  push(0); // r9
  push(0); // r10
  push(0); // r11
  push(0); // r12
  push(0); // r13
  push(0); // r14
  push(0); // r15

  return *this;
}

uint64_t& Task::StackPointer() {
  return stack_ptr_;
}

uint64_t Task::ID() const {
  return id_;
}

void Task::Sleep() {
  task_manager->Sleep(this);
}

void Task::Wakeup() {
  task_manager->Wakeup(this);
}

TaskManager::TaskManager() {
  running_.push_back(&NewTask());
}

Task& TaskManager::NewTask() {
  ++latest_id_;
  return *tasks_.emplace_back(new Task{latest_id_});
}

void TaskManager::SwitchTask(bool current_sleep) {
  Task* current_task = running_.front();
  running_.pop_front();
  if (!current_sleep) {
    running_.push_back(current_task);
  }
  Task* next_task = running_.front();

  SwitchContext(&next_task->StackPointer(), &current_task->StackPointer());
}

void TaskManager::Sleep(Task* task) {
  auto it = std::find(running_.begin(), running_.end(), task);

  if (it == running_.begin()) {
    SwitchTask(true);
    return;
  }

  if (it == running_.end()) {
    return;
  }

  running_.erase(it);
}

Error TaskManager::Sleep(uint64_t id) {
  auto it = std::find_if(tasks_.begin(), tasks_.end(),
                         [id](const auto& t){ return t->ID() == id; });
  if (it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Sleep(it->get());
  return MAKE_ERROR(Error::kSuccess);
}

void TaskManager::Wakeup(Task* task) {
  auto it = std::find(running_.begin(), running_.end(), task);
  if (it == running_.end()) {
    running_.push_back(task);
  }
}

Error TaskManager::Wakeup(uint64_t id) {
  auto it = std::find_if(tasks_.begin(), tasks_.end(),
                         [id](const auto& t){ return t->ID() == id; });
  if (it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Wakeup(it->get());
  return MAKE_ERROR(Error::kSuccess);
}

TaskManager* task_manager;

void StartTask(uint64_t task_id, int64_t data, TaskFunc* f) {
  __asm__("sti");
  f(task_id, data);
  while (1) __asm__("hlt");
}

void InitializeTask() {
  task_manager = new TaskManager;

  __asm__("cli");
  timer_manager->AddTimer(
      Timer{timer_manager->CurrentTick() + kTaskTimerPeriod, kTaskTimerValue});
  __asm__("sti");
}
