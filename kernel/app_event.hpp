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
    kKeyPush,
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

    // #@@range_begin(key_push_event)
    struct {
      uint8_t modifier;
      uint8_t keycode;
      char ascii;
      int press; // 1: press, 0: release
    } keypush;
    // #@@range_end(key_push_event)
  } arg;
};

#ifdef __cplusplus
} // extern "C"
#endif
