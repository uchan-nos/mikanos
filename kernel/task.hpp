/**
 * @file task.hpp
 *
 * タスク管理，コンテキスト切り替えのプログラムを集めたファイル。
 */

#pragma once

#include <cstdint>

extern uint64_t task_b_rsp, task_a_rsp;

// #@@range_begin(taskfunc)
using TaskFunc = void (int, int);
// #@@range_end(taskfunc)

void SwitchTask();
void StartTask(int task_id, int data, TaskFunc* f);

void InitializeTask();
