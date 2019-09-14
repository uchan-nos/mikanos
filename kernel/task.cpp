#include "task.hpp"

#include "asmfunc.h"
#include "timer.hpp"

Task::Task(uint64_t id) : id_{id}, msgs_{} {
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

Task& Task::Sleep() {
  task_manager->Sleep(this);
  return *this;
}

Task& Task::Wakeup() {
  task_manager->Wakeup(this);
  return *this;
}

void Task::SendMessage(const Message& msg) {
  msgs_.push_back(msg);
  Wakeup();
}

std::optional<Message> Task::ReceiveMessage() {
  if (msgs_.empty()) {
    return std::nullopt;
  }

  auto m = msgs_.front();
  msgs_.pop_front();
  return m;
}

TaskManager::TaskManager() {
  Task& task = NewTask()
    .SetLevel(current_level_)
    .SetRunning(true);
  running_[current_level_].push_back(&task);
}

Task& TaskManager::NewTask() {
  ++latest_id_;
  return *tasks_.emplace_back(new Task{latest_id_});
}

void TaskManager::SwitchTask(bool current_sleep) {
  auto& level_queue = running_[current_level_];
  Task* current_task = level_queue.front();
  level_queue.pop_front();
  if (!current_sleep) {
    running_[current_task->Level()].push_back(current_task);
  }
  if (level_queue.empty()) {
    level_changed_ = true;
  }

  if (level_changed_) {
    level_changed_ = false;
    for (int lv = kMaxLevel; lv >= 0; --lv) {
      if (!running_[lv].empty()) {
        current_level_ = lv;
        break;
      }
    }
  }

  Task* next_task = running_[current_level_].front();

  SwitchContext(&next_task->StackPointer(), &current_task->StackPointer());
}

void TaskManager::Sleep(Task* task) {
  task->SetRunning(false);

  auto& level_queue = running_[task->Level()];
  auto it = std::find(level_queue.begin(), level_queue.end(), task);

  if (current_level_ == task->Level() && it == level_queue.begin()) {
    SwitchTask(true);
    return;
  }

  if (it == level_queue.end()) {
    return;
  }

  level_queue.erase(it);
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

void TaskManager::Wakeup(Task* task, int level) {
  if (task->Running()) {
    if (level >= 0 && level != task->Level()) {
      level_changed_ = true;
      task->SetLevel(level);
    }
    return;
  }

  // task is not running
  if (level < 0) {
    level = task->Level();
  }

  task->SetLevel(level);
  task->SetRunning(true);

  if (running_[level].empty()) {
    level_changed_ = true;
  }
  running_[level].push_back(task);
  return;
}

Error TaskManager::Wakeup(uint64_t id, int level) {
  auto it = std::find_if(tasks_.begin(), tasks_.end(),
                         [id](const auto& t){ return t->ID() == id; });
  if (it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Wakeup(it->get(), level);
  return MAKE_ERROR(Error::kSuccess);
}

Error TaskManager::SendMessage(uint64_t id, const Message& msg) {
  auto it = std::find_if(tasks_.begin(), tasks_.end(),
                         [id](const auto& t){ return t->ID() == id; });
  if (it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  (*it)->SendMessage(msg);
  return MAKE_ERROR(Error::kSuccess);
}

Task& TaskManager::CurrentTask() {
  return *running_[current_level_].front();
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
