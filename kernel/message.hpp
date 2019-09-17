#pragma once

// #@@range_begin(layer_op)
enum class LayerOperation {
  Move, MoveRelative, Draw
};
// #@@range_end(layer_op)

// #@@range_begin(msg_head)
struct Message {
  enum Type {
    kInterruptXHCI,
    kTimerTimeout,
    kKeyPush,
    kLayer,
    kLayerFinish,
  } type;

  uint64_t src_task;
// #@@range_end(msg_head)

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
    } layer;
    // #@@range_end(msg_layer)
  } arg;
};
