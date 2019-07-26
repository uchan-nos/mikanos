/**
 * @file window.hpp
 *
 * 表示領域を表すWindowクラスを提供する。
 */

#pragma once

#include <vector>
#include <optional>
#include "graphics.hpp"

/** @brief Bitmap2D クラスは平面の描画領域を提供する。
 */
// #@@range_begin(bitmap2d)
class Bitmap2D {
 public:
  /** @brief BitmapWriter は Bitmap2D と関連付けられた PixelWriter を提供する。
   */
  class BitmapWriter : public PixelWriter {
   public:
    BitmapWriter(Bitmap2D& bitmap);
    /** @brief 指定された位置に指定された色を描く */
    virtual void Write(int x, int y, const PixelColor& c) override;
    /** @brief Width は関連付けられた Bitmap2D の横幅をピクセル単位で返す。 */
    virtual int Width() const override;
    /** @brief Height は関連付けられた Bitmap2D の高さをピクセル単位で返す。 */
    virtual int Height() const override;

   private:
    Bitmap2D& bitmap_;
  };

  /** @brief 指定されたピクセル数の平面描画領域を作成する。 */
  Bitmap2D(int width, int height);
  ~Bitmap2D();
  Bitmap2D(Bitmap2D&& rhs);
  Bitmap2D& operator=(Bitmap2D&& rhs);

  /** @brief 指定した位置のピクセルを返す。 */
  PixelColor& At(int x, int y);
  /** @brief 指定した位置のピクセルを返す。 */
  const PixelColor& At(int x, int y) const;
  /** @brief このインスタンスに紐付いた BitmapWriter を取得する。 */
  BitmapWriter* PixelWriter();

  /** @brief 平面描画領域の横幅をピクセル単位で返す。 */
  int Width() const;
  /** @brief 平面描画領域の高さをピクセル単位で返す。 */
  int Height() const;

 private:
  int width_, height_;
  std::vector<std::vector<PixelColor>> data_;
  BitmapWriter writer_{*this};
};
// #@@range_end(bitmap2d)

/** @brief Window クラスはグラフィックの表示領域を表す。
 *
 * タイトルやメニューがあるウィンドウだけでなく，マウスカーソルの表示領域なども対象とする。
 */
// #@@range_begin(window)
class Window {
 public:
  /** @brief 指定した Bitmap2D を表示領域として持つウィンドウを生成する。 */
  Window(Bitmap2D&& bitmap);
  /** @brief このウィンドウの表示領域を返す。 */
  Bitmap2D& Bitmap();
  /** @brief 与えられた PixelWriter にこのウィンドウの表示領域を描画する。
   *
   * @param writer  描画先
   * @param position  writer の左上を基準とした描画位置
   */
  void DrawTo(PixelWriter& writer, Vector2D<int> position);
  /** @brief 透過色を設定する。 */
  void SetTransparentColor(std::optional<PixelColor> c);

 private:
  Bitmap2D bitmap_;
  std::optional<PixelColor> transparent_color_{std::nullopt};
};
// #@@range_end(window)
