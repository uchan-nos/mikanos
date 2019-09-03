#pragma once

struct Message {
  enum Type {
    kInterruptXHCI,
    kTimerTimeout,
    kKeyPush,
  } type;

  union {
    struct {
      unsigned long timeout;
      int value;
    } timer;

    struct {
      uint8_t keycode;
      char ascii;
    } keyboard;
  } arg;
};
