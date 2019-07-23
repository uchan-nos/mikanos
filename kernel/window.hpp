/**
 * @file window.hpp
 *
 * 表示領域を表すWindowクラスを提供する。
 */

#pragma once

#include <vector>
#include "graphics.hpp"

class Bitmap2D {
 public:
  class BitmapWriter : public PixelWriter {
   public:
    BitmapWriter(Bitmap2D& bitmap) : bitmap_{bitmap} {
    }

    virtual void Write(int x, int y, const PixelColor& c) override {
      bitmap_.At(x, y) = c;
    }

    virtual int Width() const override {
      return bitmap_.Width();
    }

    virtual int Height() const override {
      return bitmap_.Height();
    }

   private:
    Bitmap2D& bitmap_;
  };

  Bitmap2D(int width, int height) : width_{width}, height_{height}, data_(height, std::vector<PixelColor>(width, PixelColor{})), writer_{*this} {
  }
  ~Bitmap2D() = default;
  Bitmap2D(Bitmap2D&& rhs) : width_{rhs.width_}, height_{rhs.height_}, data_{std::move(rhs.data_)}, writer_{*this} {
  }
  Bitmap2D& operator=(Bitmap2D&& rhs) {
    width_ = rhs.width_;
    height_ = rhs.height_;
    data_ = std::move(rhs.data_);
    return *this;
  }

  PixelColor& At(int x, int y) { return data_[y][x]; }
  const PixelColor& At(int x, int y) const { return data_[y][x]; }
  BitmapWriter* PixelWriter() {
    return &writer_;
  }

  int Width() const { return width_; }
  int Height() const { return height_; }

 private:
  int width_, height_;
  std::vector<std::vector<PixelColor>> data_;
  BitmapWriter writer_{*this};
};

/** @brief Windows クラスはグラフィックの表示領域を表す。
 *
 * タイトルやメニューがあるウィンドウだけでなく，マウスカーソルの表示領域なども対象とする。
 */
class Window {
 public:
  Window(Bitmap2D&& bitmap) : bitmap_{std::move(bitmap)} {
  }

  Bitmap2D& Bitmap() {
    return bitmap_;
  }

  void DrawTo(PixelWriter& writer, Vector2D<int> center_position) {
    int origin_x = center_position.x - bitmap_.Width() / 2;
    int origin_y = center_position.y - bitmap_.Height() / 2;

    if (transparent_color_) {
      const auto tc = transparent_color_.value();
      for (int y = 0; y < bitmap_.Height(); ++y) {
        for (int x = 0; x < bitmap_.Width(); ++x) {
          const auto c = bitmap_.At(x, y);
          if (c != tc) {
            writer.Write(origin_x + x, origin_y + y, c);
          }
        }
      }
      return;
    }

    for (int y = 0; y < bitmap_.Height(); ++y) {
      for (int x = 0; x < bitmap_.Width(); ++x) {
        auto c = bitmap_.At(x, y);
        writer.Write(origin_x + x, origin_y + y, c);
      }
    }
  }

  void SetTransparentColor(PixelColor c) {
    transparent_color_ = c;
  }

 private:
  Bitmap2D bitmap_;
  std::optional<PixelColor> transparent_color_{std::nullopt};
};
