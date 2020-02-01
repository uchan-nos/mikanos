#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AppEvent {
  // #@@range_begin(app_event_type)
  enum Type {
    kQuit,
    kMouseMove,
    kMouseButton,
  } type;
  // #@@range_end(app_event_type)

  union {
    struct {
      int x, y;
      int dx, dy;
      uint8_t buttons;
    } mouse_move;

    // #@@range_begin(mouse_button_arg)
    struct {
      int x, y;
      int press; // 1: press, 0: release
      int button;
    } mouse_button;
    // #@@range_end(mouse_button_arg)
  } arg;
};

#ifdef __cplusplus
} // extern "C"
#endif
