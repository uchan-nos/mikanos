/**
 * @file keyboard.hpp
 *
 * キーボード制御プログラム．
 */

#pragma once

#include <cstdint>

class Keyboard {
 public:
  void OnInterrupt(uint8_t keycode);
};

void InitializeKeyboard();
