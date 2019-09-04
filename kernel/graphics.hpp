#pragma once

#include <algorithm>
#include <cstdint>
#include "frame_buffer_config.hpp"

struct PixelColor {
  uint8_t r, g, b;
};

constexpr PixelColor ToColor(uint32_t c) {
  return {
    static_cast<uint8_t>((c >> 16) & 0xff),
    static_cast<uint8_t>((c >> 8) & 0xff),
    static_cast<uint8_t>(c & 0xff)
  };
}

inline bool operator==(const PixelColor& lhs, const PixelColor& rhs) {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

inline bool operator!=(const PixelColor& lhs, const PixelColor& rhs) {
  return !(lhs == rhs);
}

template <typename T>
struct Vector2D {
  T x, y;

  template <typename U>
  Vector2D<T>& operator +=(const Vector2D<U>& rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  template <typename U>
  Vector2D<T> operator +(const Vector2D<U>& rhs) const {
    auto tmp = *this;
    tmp += rhs;
    return tmp;
  }

  template <typename U>
  Vector2D<T>& operator -=(const Vector2D<U>& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }

  template <typename U>
  Vector2D<T> operator -(const Vector2D<U>& rhs) const {
    auto tmp = *this;
    tmp -= rhs;
    return tmp;
  }
};

template <typename T>
Vector2D<T> ElementMax(const Vector2D<T>& lhs, const Vector2D<T>& rhs) {
  return {std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y)};
}

template <typename T>
Vector2D<T> ElementMin(const Vector2D<T>& lhs, const Vector2D<T>& rhs) {
  return {std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y)};
}

template <typename T>
struct Rectangle {
  Vector2D<T> pos, size;
};

template <typename T, typename U>
Rectangle<T> operator&(const Rectangle<T>& lhs, const Rectangle<U>& rhs) {
  const auto lhs_end = lhs.pos + lhs.size;
  const auto rhs_end = rhs.pos + rhs.size;
  if (lhs_end.x < rhs.pos.x || lhs_end.y < rhs.pos.y ||
      rhs_end.x < lhs.pos.x || rhs_end.y < lhs.pos.y) {
    return {{0, 0}, {0, 0}};
  }

  auto new_pos = ElementMax(lhs.pos, rhs.pos);
  auto new_size = ElementMin(lhs_end, rhs_end) - new_pos;
  return {new_pos, new_size};
}

class PixelWriter {
 public:
  virtual ~PixelWriter() = default;
  virtual void Write(Vector2D<int> pos, const PixelColor& c) = 0;
  virtual int Width() const = 0;
  virtual int Height() const = 0;
};

class FrameBufferWriter : public PixelWriter {
 public:
  FrameBufferWriter(const FrameBufferConfig& config) : config_{config} {
  }
  virtual ~FrameBufferWriter() = default;
  virtual int Width() const override { return config_.horizontal_resolution; }
  virtual int Height() const override { return config_.vertical_resolution; }

 protected:
  uint8_t* PixelAt(Vector2D<int> pos) {
    return config_.frame_buffer + 4 * (config_.pixels_per_scan_line * pos.y + pos.x);
  }

 private:
  const FrameBufferConfig& config_;
};

class RGBResv8BitPerColorPixelWriter : public FrameBufferWriter {
 public:
  using FrameBufferWriter::FrameBufferWriter;
  virtual void Write(Vector2D<int> pos, const PixelColor& c) override;
};

class BGRResv8BitPerColorPixelWriter : public FrameBufferWriter {
 public:
  using FrameBufferWriter::FrameBufferWriter;
  virtual void Write(Vector2D<int> pos, const PixelColor& c) override;
};

void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c);

void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c);

const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};

void DrawDesktop(PixelWriter& writer);

extern FrameBufferConfig screen_config;
extern PixelWriter* screen_writer;
Vector2D<int> ScreenSize();

void InitializeGraphics(const FrameBufferConfig& screen_config);
