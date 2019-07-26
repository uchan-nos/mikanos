#include "window.hpp"

// #@@range_begin(bitmapwriter_ctor)
Bitmap2D::BitmapWriter::BitmapWriter(Bitmap2D& bitmap) : bitmap_{bitmap} {
}
// #@@range_end(bitmapwriter_ctor)

// #@@range_begin(bitmapwriter_write)
void Bitmap2D::BitmapWriter::Write(int x, int y, const PixelColor& c) {
  bitmap_.At(x, y) = c;
}
// #@@range_end(bitmapwriter_write)

// #@@range_begin(bitmapwriter_width_height)
int Bitmap2D::BitmapWriter::Width() const {
  return bitmap_.Width();
}

int Bitmap2D::BitmapWriter::Height() const {
  return bitmap_.Height();
}
// #@@range_end(bitmapwriter_width_height)


// #@@range_begin(bitmap_ctor)
Bitmap2D::Bitmap2D(int width, int height)
  : width_{width}, height_{height},
    data_(height, std::vector<PixelColor>(width, PixelColor{})),
    writer_{*this} {
}
// #@@range_end(bitmap_ctor)

// #@@range_begin(bitmap_dtor)
Bitmap2D::~Bitmap2D() = default;
// #@@range_end(bitmap_dtor)

// #@@range_begin(bitmap_movector)
Bitmap2D::Bitmap2D(Bitmap2D&& rhs)
  : width_{rhs.width_}, height_{rhs.height_},
    data_{std::move(rhs.data_)},
    writer_{*this} {
}
// #@@range_end(bitmap_movector)

// #@@range_begin(bitmap_moveassign)
Bitmap2D& Bitmap2D::operator=(Bitmap2D&& rhs) {
  width_ = rhs.width_;
  height_ = rhs.height_;
  data_ = std::move(rhs.data_);
  return *this;
}
// #@@range_end(bitmap_moveassign)

// #@@range_begin(bitmap_at)
PixelColor& Bitmap2D::At(int x, int y) {
  return data_[y][x];
}
const PixelColor& Bitmap2D::At(int x, int y) const {
  return data_[y][x];
}
// #@@range_end(bitmap_at)
// #@@range_begin(bitmap_getwriter)
Bitmap2D::BitmapWriter* Bitmap2D::PixelWriter() {
  return &writer_;
}
// #@@range_end(bitmap_getwriter)

// #@@range_begin(bitmap_width_heigit)
int Bitmap2D::Width() const {
  return width_;
}
int Bitmap2D::Height() const {
  return height_;
}
// #@@range_end(bitmap_width_heigit)


// #@@range_begin(window_ctor)
Window::Window(Bitmap2D&& bitmap) : bitmap_{std::move(bitmap)} {
}
// #@@range_end(window_ctor)

// #@@range_begin(window_getbitmap)
Bitmap2D& Window::Bitmap() {
  return bitmap_;
}
// #@@range_end(window_getbitmap)

// #@@range_begin(window_drawto)
void Window::DrawTo(PixelWriter& writer, Vector2D<int> position) {
  if (transparent_color_) {
    const auto tc = transparent_color_.value();
    for (int y = 0; y < bitmap_.Height(); ++y) {
      for (int x = 0; x < bitmap_.Width(); ++x) {
        const auto c = bitmap_.At(x, y);
        if (c != tc) {
          writer.Write(position.x + x, position.y + y, c);
        }
      }
    }
    return;
  }

  for (int y = 0; y < bitmap_.Height(); ++y) {
    for (int x = 0; x < bitmap_.Width(); ++x) {
      auto c = bitmap_.At(x, y);
      writer.Write(position.x + x, position.y + y, c);
    }
  }
}
// #@@range_end(window_drawto)

// #@@range_begin(window_settc)
void Window::SetTransparentColor(std::optional<PixelColor> c) {
  transparent_color_ = c;
}
// #@@range_end(window_settc)
