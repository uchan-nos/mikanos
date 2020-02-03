#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AppEvent {
  enum Type {
    kQuit,
    kMouseMove,
    kMouseButton,
    kTimerTimeout,
  } type;

  union {
    struct {
      int x, y;
      int dx, dy;
      uint8_t buttons;
    } mouse_move;

    struct {
      int x, y;
      int press; // 1: press, 0: release
      int button;
    } mouse_button;

    struct {
      unsigned long timeout;
      int value;
    } timer;
  } arg;
};

#ifdef __cplusplus
} // extern "C"
#endif
