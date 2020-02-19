#pragma once

enum class LayerOperation {
  Move, MoveRelative, Draw, DrawArea
};

struct Message {
  enum Type {
    kInterruptXHCI,
    kTimerTimeout,
    kKeyPush,
    kLayer,
    kLayerFinish,
    kMouseMove,
    kMouseButton,
    kWindowActive,
  } type;

  uint64_t src_task;

  union {
    struct {
      unsigned long timeout;
      int value;
    } timer;

    struct {
      uint8_t modifier;
      uint8_t keycode;
      char ascii;
      int press;
    } keyboard;

    struct {
      LayerOperation op;
      unsigned int layer_id;
      int x, y;
      int w, h;
    } layer;

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

    // #@@range_begin(winact_struct)
    struct {
      int activate; // 1: activate, 0: deactivate
    } window_active;
    // #@@range_end(winact_struct)
  } arg;
};
