#pragma once

// #@@range_begin(layer_op)
enum class LayerOperation {
  Move, MoveRelative, Draw, DrawArea
};
// #@@range_end(layer_op)

struct Message {
  enum Type {
    kInterruptXHCI,
    kTimerTimeout,
    kKeyPush,
    kLayer,
    kLayerFinish,
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
    } keyboard;

    // #@@range_begin(msg_layer)
    struct {
      LayerOperation op;
      unsigned int layer_id;
      int x, y;
      int w, h;
    } layer;
    // #@@range_end(msg_layer)
  } arg;
};
