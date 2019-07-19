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
    BitmapWriter(Bitmap2D* bitmap) : bitmap_{bitmap} {
    }

    virtual void Write(int x, int y, const PixelColor& c) override {
      bitmap_->At(x, y) = c;
    }

   private:
    Bitmap2D* bitmap_;
  };

  Bitmap2D(int width, int height) : width_{width}, height_{height}, data_(height, std::vector<PixelColor>(width, PixelColor{})) {
  }

  PixelColor& At(int x, int y) { return data_[y][x]; }
  const PixelColor& At(int x, int y) const { return data_[y][x]; }
  BitmapWriter NewWriter() {
    return BitmapWriter{this};
  }

  int Width() const { return width_; }
  int Height() const { return height_; }

 private:
  int width_, height_;
  std::vector<std::vector<PixelColor>> data_;
};

/** @brief Windows クラスはグラフィックの表示領域を表す。
 *
 * タイトルやメニューがあるウィンドウだけでなく，マウスカーソルの表示領域なども対象とする。
 */
class Window {
 public:
  Window(const std::shared_ptr<Bitmap2D>& bitmap) : bitmap_{bitmap} {
  }

  void DrawTo(PixelWriter& writer, Vector2D<int> center_position) {
    int origin_x = center_position.x - bitmap_->Width() / 2;
    int origin_y = center_position.y - bitmap_->Height() / 2;

    for (int y = 0; y < bitmap_->Height(); ++y) {
      for (int x = 0; x < bitmap_->Width(); ++x) {
        writer.Write(origin_x + x, origin_y + y, bitmap_->At(x, y));
      }
    }
  }

 private:
  std::shared_ptr<Bitmap2D> bitmap_;

  std::optional<PixelColor> transparent_color_;
};
