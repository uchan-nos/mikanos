#include "timer.hpp"

#include "acpi.hpp"
#include "interrupt.hpp"

namespace {
  const uint32_t kCountMax = 0xffffffffu;
  volatile uint32_t& lvt_timer = *reinterpret_cast<uint32_t*>(0xfee00320);
  volatile uint32_t& initial_count = *reinterpret_cast<uint32_t*>(0xfee00380);
  volatile uint32_t& current_count = *reinterpret_cast<uint32_t*>(0xfee00390);
  volatile uint32_t& divide_config = *reinterpret_cast<uint32_t*>(0xfee003e0);
}

// #@@range_begin(init_timer)
void InitializeLAPICTimer(std::deque<Message>& msg_queue) {
  timer_manager = new TimerManager{msg_queue};

  divide_config = 0b1011; // divide 1:1
  lvt_timer = 0b001 << 16; // masked, one-shot

  StartLAPICTimer();
  acpi::WaitMilliseconds(100);
  const auto elapsed = LAPICTimerElapsed();
  StopLAPICTimer();

  lapic_timer_freq = static_cast<unsigned long>(elapsed) * 10;

  divide_config = 0b1011; // divide 1:1
  lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer; // not-masked, periodic
  initial_count = lapic_timer_freq / kTimerFreq;
}
// #@@range_end(init_timer)

void StartLAPICTimer() {
  initial_count = kCountMax;
}

uint32_t LAPICTimerElapsed() {
  return kCountMax - current_count;
}

void StopLAPICTimer() {
  initial_count = 0;
}

Timer::Timer(unsigned long timeout, int value)
    : timeout_{timeout}, value_{value} {
}

TimerManager::TimerManager(std::deque<Message>& msg_queue)
    : msg_queue_{msg_queue} {
  timers_.push(Timer{std::numeric_limits<unsigned long>::max(), -1});
}

void TimerManager::AddTimer(const Timer& timer) {
  timers_.push(timer);
}

void TimerManager::Tick() {
  ++tick_;
  while (true) {
    const auto& t = timers_.top();
    if (t.Timeout() > tick_) {
      break;
    }

    Message m{Message::kTimerTimeout};
    m.arg.timer.timeout = t.Timeout();
    m.arg.timer.value = t.Value();
    msg_queue_.push_back(m);

    timers_.pop();
  }
}

TimerManager* timer_manager;
unsigned long lapic_timer_freq;

void LAPICTimerOnInterrupt() {
  timer_manager->Tick();
}
