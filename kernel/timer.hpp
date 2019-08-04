#pragma once

#include <cstdint>

namespace {
  const uint32_t kCountMax = 0xffffffffu;
  volatile uint32_t& lvt_timer = *reinterpret_cast<uint32_t*>(0xfee00320);
  volatile uint32_t& initial_count = *reinterpret_cast<uint32_t*>(0xfee00380);
  volatile uint32_t& current_count = *reinterpret_cast<uint32_t*>(0xfee00390);
  volatile uint32_t& divide_config = *reinterpret_cast<uint32_t*>(0xfee003e0);
}

inline void InitializeLAPICTimer() {
  divide_config = 0b1011; // divide 1:1
  lvt_timer = (0b001 << 16) | 32; // masked, one-shot
}

inline void StartLAPICTimer() {
  initial_count = kCountMax;
}

inline uint32_t LAPICTimerElapsed() {
  return kCountMax - current_count;
}

inline void StopLAPICTimer() {
  initial_count = 0;
}
