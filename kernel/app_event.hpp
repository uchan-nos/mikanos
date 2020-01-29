#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// #@@range_begin(app_event)
struct AppEvent {
  enum Type {
    kQuit,
    kMouseMove,
  } type;

  union {
    struct {
      int x, y;
      int dx, dy;
      uint8_t buttons;
    } mouse_move;
  } arg;
};
// #@@range_end(app_event)

#ifdef __cplusplus
} // extern "C"
#endif
