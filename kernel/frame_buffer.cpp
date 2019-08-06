#include "frame_buffer.hpp"

namespace {
  int BytesPerPixel(PixelFormat format) {
    switch (format) {
      case kPixelRGBResv8BitPerColor: return 4;
      case kPixelBGRResv8BitPerColor: return 4;
    }
    return -1;
  }

  uint8_t* FrameAddrAt(Vector2D<int> pos, const FrameBufferConfig& config) {
    return config.frame_buffer + BytesPerPixel(config.pixel_format) *
      (config.pixels_per_scan_line * pos.y + pos.x);
  }

  int BytesPerScanLine(const FrameBufferConfig& config) {
    return BytesPerPixel(config.pixel_format) * config.pixels_per_scan_line;
  }

  Vector2D<int> FrameBufferSize(const FrameBufferConfig& config) {
    return {static_cast<int>(config.horizontal_resolution),
            static_cast<int>(config.vertical_resolution)};
  }
}

Error FrameBuffer::Initialize(const FrameBufferConfig& config) {
  config_ = config;

  const auto bytes_per_pixel = BytesPerPixel(config_.pixel_format);
  if (bytes_per_pixel <= 0) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  if (config_.frame_buffer) {
    buffer_.resize(0);
  } else {
    buffer_.resize(
        bytes_per_pixel
        * config_.horizontal_resolution * config_.vertical_resolution);
    config_.frame_buffer = buffer_.data();
    config_.pixels_per_scan_line = config_.horizontal_resolution;
  }

  switch (config_.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      writer_ = std::make_unique<RGBResv8BitPerColorPixelWriter>(config_);
      break;
    case kPixelBGRResv8BitPerColor:
      writer_ = std::make_unique<BGRResv8BitPerColorPixelWriter>(config_);
      break;
    default:
      return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  return MAKE_ERROR(Error::kSuccess);
}

Error FrameBuffer::Copy(Vector2D<int> dst_pos, const FrameBuffer& src) {
  if (config_.pixel_format != src.config_.pixel_format) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  const auto bytes_per_pixel = BytesPerPixel(config_.pixel_format);
  if (bytes_per_pixel <= 0) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  const auto dst_size = FrameBufferSize(config_);
  const auto src_size = FrameBufferSize(src.config_);

  const Vector2D<int> dst_start = ElementMax(dst_pos, {0, 0});
  const Vector2D<int> dst_end = ElementMin(dst_pos + src_size, dst_size);

  uint8_t* dst_buf = FrameAddrAt(dst_start, config_);
  const uint8_t* src_buf = FrameAddrAt({0, 0}, src.config_);

  for (int y = dst_start.y; y < dst_end.y; ++y) {
    memcpy(dst_buf, src_buf, bytes_per_pixel * (dst_end.x - dst_start.x));
    dst_buf += BytesPerScanLine(config_);
    src_buf += BytesPerScanLine(src.config_);
  }

  return MAKE_ERROR(Error::kSuccess);
}

void FrameBuffer::Move(Vector2D<int> dst_pos, const Rectangle<int>& src) {
  const auto bytes_per_pixel = BytesPerPixel(config_.pixel_format);
  const auto bytes_per_scan_line = BytesPerScanLine(config_);

  if (dst_pos.y < src.pos.y) { // move up
    uint8_t* dst_buf = FrameAddrAt(dst_pos, config_);
    const uint8_t* src_buf = FrameAddrAt(src.pos, config_);
    for (int y = 0; y < src.size.y; ++y) {
      memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
      dst_buf += bytes_per_scan_line;
      src_buf += bytes_per_scan_line;
    }
  } else { // move down
    uint8_t* dst_buf = FrameAddrAt(dst_pos + Vector2D<int>{0, src.size.y - 1}, config_);
    const uint8_t* src_buf = FrameAddrAt(src.pos + Vector2D<int>{0, src.size.y - 1}, config_);
    for (int y = 0; y < src.size.y; ++y) {
      memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
      dst_buf -= bytes_per_scan_line;
      src_buf -= bytes_per_scan_line;
    }
  }
}
