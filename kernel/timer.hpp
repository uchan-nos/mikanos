#pragma once

#include <cstdint>
#include <queue>
#include <vector>
#include "message.hpp"

void InitializeLAPICTimer(std::deque<Message>& msg_queue);
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();

// #@@range_begin(timer)
class Timer {
 public:
  Timer(unsigned long timeout, int value);
  unsigned long Timeout() const { return timeout_; }
  int Value() const { return value_; }

 private:
  unsigned long timeout_;
  int value_;
};
// #@@range_end(timer)

// #@@range_begin(timer_less)
/** @brief タイマー優先度を比較する。タイムアウトが遠いほど優先度低。 */
inline bool operator<(const Timer& lhs, const Timer& rhs) {
  return lhs.Timeout() > rhs.Timeout();
}
// #@@range_end(timer_less)

// #@@range_begin(timermgr)
class TimerManager {
 public:
  TimerManager(std::deque<Message>& msg_queue);
  void AddTimer(const Timer& timer);
  void Tick();
  unsigned long CurrentTick() const { return tick_; }

 private:
  volatile unsigned long tick_{0};
  std::priority_queue<Timer> timers_{};
  std::deque<Message>& msg_queue_;
};
// #@@range_end(timermgr)

extern TimerManager* timer_manager;

void LAPICTimerOnInterrupt();
