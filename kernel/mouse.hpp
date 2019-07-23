/**
 * @file mouse.hpp
 *
 * マウス制御プログラム．
 */

#pragma once

#include "graphics.hpp"

const int kMouseCursorWidth = 15;
const int kMouseCursorHeight = 24;
const PixelColor kMouseTransparentColor{0, 0, 1};

void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position);
