/**
 * @file mouse.hpp
 *
 * マウス制御プログラム．
 */

#pragma once

// #@@range_begin(mouse_class)
#include "graphics.hpp"

class MouseCursor {
 public:
  MouseCursor(PixelWriter* writer, PixelColor erase_color,
              Vector2D<int> initial_position);
  void MoveRelative(Vector2D<int> displacement);

 private:
  PixelWriter* pixel_writer_ = nullptr;
  PixelColor erase_color_;
  Vector2D<int> position_;
};
// #@@range_end(mouse_class)
