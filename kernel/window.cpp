#include "window.hpp"

Bitmap2D::BitmapWriter::BitmapWriter(Bitmap2D& bitmap) : bitmap_{bitmap} {
}

void Bitmap2D::BitmapWriter::Write(int x, int y, const PixelColor& c) {
  bitmap_.At(x, y) = c;
}

int Bitmap2D::BitmapWriter::Width() const {
  return bitmap_.Width();
}

int Bitmap2D::BitmapWriter::Height() const {
  return bitmap_.Height();
}


Bitmap2D::Bitmap2D(int width, int height)
  : width_{width}, height_{height},
    data_(height, std::vector<PixelColor>(width, PixelColor{})),
    writer_{*this} {
}

Bitmap2D::~Bitmap2D() = default;

Bitmap2D::Bitmap2D(Bitmap2D&& rhs)
  : width_{rhs.width_}, height_{rhs.height_},
    data_{std::move(rhs.data_)},
    writer_{*this} {
}

Bitmap2D& Bitmap2D::operator=(Bitmap2D&& rhs) {
  width_ = rhs.width_;
  height_ = rhs.height_;
  data_ = std::move(rhs.data_);
  return *this;
}

PixelColor& Bitmap2D::At(int x, int y) {
  return data_[y][x];
}
const PixelColor& Bitmap2D::At(int x, int y) const {
  return data_[y][x];
}
Bitmap2D::BitmapWriter* Bitmap2D::PixelWriter() {
  return &writer_;
}

int Bitmap2D::Width() const {
  return width_;
}
int Bitmap2D::Height() const {
  return height_;
}


Window::Window(Bitmap2D&& bitmap) : bitmap_{std::move(bitmap)} {
}

Bitmap2D& Window::Bitmap() {
  return bitmap_;
}

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

void Window::SetTransparentColor(std::optional<PixelColor> c) {
  transparent_color_ = c;
}
