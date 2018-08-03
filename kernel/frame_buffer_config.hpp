#pragma once

#include <stdint.h>

enum PixelFormat {
  PixelRGBResv8BitPerColor,
  PixelBGRResv8BitPerColor,
};

struct FrameBufferConfig {
  uint8_t* frame_buffer;
  uint32_t pixels_per_scan_line;
  uint32_t horizontal_resolution;
  uint32_t vertical_resolution;
  enum PixelFormat pixel_format;
};
