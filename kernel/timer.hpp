#pragma once

#include <cstdint>
#include <queue>
#include <vector>
#include "message.hpp"

void InitializeLAPICTimer(std::deque<Message>& msg_queue);
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();

class Timer {
 public:
  Timer(unsigned long timeout, int value);
  unsigned long Timeout() const { return timeout_; }
  int Value() const { return value_; }

 private:
  unsigned long timeout_;
  int value_;
};

/** @brief タイマー優先度を比較する。タイムアウトが遠いほど優先度低。 */
inline bool operator<(const Timer& lhs, const Timer& rhs) {
  return lhs.Timeout() > rhs.Timeout();
}

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

// #@@range_begin(lapic_freq)
extern TimerManager* timer_manager;
extern unsigned long lapic_timer_freq;
const int kTimerFreq = 100;
// #@@range_end(lapic_freq)

void LAPICTimerOnInterrupt();
