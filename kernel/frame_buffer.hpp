#pragma once

#include <vector>
#include <memory>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "error.hpp"

class FrameBuffer {
 public:
  Error Initialize(const FrameBufferConfig& config);
  Error Copy(Vector2D<int> pos, const FrameBuffer& src);

  FrameBufferWriter& Writer() { return *writer_; }

 private:
  FrameBufferConfig config_{};
  std::vector<uint8_t> buffer_{};
  std::unique_ptr<FrameBufferWriter> writer_{};
};

int BitsPerPixel(PixelFormat format);
