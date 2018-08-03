#include <cstdint>
#include <cstddef>

#include "frame_buffer_config.hpp"

struct PixelColor {
  uint8_t r, g, b;
};

int WritePixel(const FrameBufferConfig& config,
                int x, int y, const PixelColor& c) {
  if (config.pixel_format == PixelRGBResv8BitPerColor) {
    uint8_t* p = &config.frame_buffer[4 * (config.pixels_per_scan_line * y + x)];
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
  } else if (config.pixel_format == PixelBGRResv8BitPerColor) {
    uint8_t* p = &config.frame_buffer[4 * (config.pixels_per_scan_line * y + x)];
    p[0] = c.b;
    p[1] = c.g;
    p[2] = c.r;
  } else {
    return -1;
  }
  return 0;
}

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
  for (int x = 0; x < frame_buffer_config.horizontal_resolution; ++x) {
    for (int y = 0; y < frame_buffer_config.vertical_resolution; ++y) {
      uint8_t c = (x + y) % 256;
      WritePixel(frame_buffer_config, x, y, {c, c, c});
    }
  }

  for (int x = 0; x < 50; ++x) {
    for (int y = 0; y < 30; ++y) {
      WritePixel(frame_buffer_config, x, y, {255, 0, 0});
    }
  }
  while (1) __asm__("hlt");
}
