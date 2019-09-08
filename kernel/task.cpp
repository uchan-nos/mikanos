#include "task.hpp"

#include "asmfunc.h"
#include "timer.hpp"

// #@@range_begin(switchtask)
uint64_t task_b_rsp, task_a_rsp;

namespace {
  uint64_t* current_task;
}

void SwitchTask() {
  uint64_t* old_current_task = current_task;
  if (current_task == &task_a_rsp) {
    current_task = &task_b_rsp;
  } else {
    current_task = &task_a_rsp;
  }
  SwitchContext(current_task, old_current_task);
}
// #@@range_end(switchtask)

// #@@range_begin(starttask)
void StartTask(int task_id, int data, TaskFunc* f) {
  __asm__("sti");
  f(task_id, data);
  while (1) __asm__("hlt");
}
// #@@range_end(starttask)

// #@@range_begin(inittask)
void InitializeTask() {
  current_task = &task_a_rsp;

  __asm__("cli");
  timer_manager->AddTimer(
      Timer{timer_manager->CurrentTick() + kTaskTimerPeriod, kTaskTimerValue});
  __asm__("sti");
}
// #@@range_end(inittask)
