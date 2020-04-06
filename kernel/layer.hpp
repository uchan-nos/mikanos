/**
 * @file layer.hpp
 *
 * 重ね合わせ処理を提供する。
 */

#pragma once

#include <memory>
#include <map>
#include <vector>

#include "graphics.hpp"
#include "window.hpp"
#include "message.hpp"

/** @brief Layer は 1 つの層を表す。
 *
 * 現状では 1 つのウィンドウしか保持できない設計だが，
 * 将来的には複数のウィンドウを持ち得る。
 */
class Layer {
 public:
  /** @brief 指定された ID を持つレイヤーを生成する。 */
  Layer(unsigned int id = 0);
  /** @brief このインスタンスの ID を返す。 */
  unsigned int ID() const;

  /** @brief ウィンドウを設定する。既存のウィンドウはこのレイヤーから外れる。 */
  Layer& SetWindow(const std::shared_ptr<Window>& window);
  /** @brief 設定されたウィンドウを返す。 */
  std::shared_ptr<Window> GetWindow() const;
  /** @brief レイヤーの原点座標を取得する。 */
  Vector2D<int> GetPosition() const;
  /** @brief true でレイヤーがドラッグ移動可能となる。 */
  Layer& SetDraggable(bool draggable);
  /** @brief レイヤーがドラッグ移動可能なら true を返す。 */
  bool IsDraggable() const;

  /** @brief レイヤーの位置情報を指定された絶対座標へと更新する。再描画はしない。 */
  Layer& Move(Vector2D<int> pos);
  /** @brief レイヤーの位置情報を指定された相対座標へと更新する。再描画はしない。 */
  Layer& MoveRelative(Vector2D<int> pos_diff);

  /** @brief 指定された描画先にウィンドウの内容を描画する。 */
  void DrawTo(FrameBuffer& screen, const Rectangle<int>& area) const;

 private:
  unsigned int id_;
  Vector2D<int> pos_{};
  std::shared_ptr<Window> window_{};
  bool draggable_{false};
};

/** @brief LayerManager は複数のレイヤーを管理する。 */
class LayerManager {
 public:
  /** @brief Draw メソッドなどで描画する際の描画先を設定する。 */
  void SetWriter(FrameBuffer* screen);
  /** @brief 新しいレイヤーを生成して参照を返す。
   *
   * 新しく生成されたレイヤーの実体は LayerManager 内部のコンテナで保持される。
   */
  Layer& NewLayer();
  /** @brief 指定されたレイヤーを削除する。 */
  void RemoveLayer(unsigned int id);

  /** @brief 現在表示状態にあるレイヤーを描画する。 */
  void Draw(const Rectangle<int>& area) const;
  /** @brief 指定したレイヤーに設定されているウィンドウの描画領域内を再描画する。 */
  void Draw(unsigned int id) const;
  /** @brief 指定したレイヤーに設定されているウィンドウ内の指定された範囲を再描画する。 */
  void Draw(unsigned int id, Rectangle<int> area) const;

  /** @brief レイヤーの位置情報を指定された絶対座標へと更新する。再描画する。 */
  void Move(unsigned int id, Vector2D<int> new_pos);
  /** @brief レイヤーの位置情報を指定された相対座標へと更新する。再描画する。 */
  void MoveRelative(unsigned int id, Vector2D<int> pos_diff);

  /** @brief レイヤーの高さ方向の位置を指定された位置に移動する。
   *
   * new_height に負の高さを指定するとレイヤーは非表示となり，
   * 0 以上を指定するとその高さとなる。
   * 現在のレイヤー数以上の数値を指定した場合は最前面のレイヤーとなる。
   * */
  void UpDown(unsigned int id, int new_height);
  /** @brief レイヤーを非表示とする。 */
  void Hide(unsigned int id);

  /** @brief 指定された座標にウィンドウを持つ最も上に表示されているレイヤーを探す。 */
  Layer* FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const;
  /** @brief 指定された ID を持つレイヤーを返す。 */
  Layer* FindLayer(unsigned int id);
  /** @brief 指定されたレイヤーの現在の高さを返す。 */
  int GetHeight(unsigned int id);

 private:
  FrameBuffer* screen_{nullptr};
  mutable FrameBuffer back_buffer_{};
  std::vector<std::unique_ptr<Layer>> layers_{};
  std::vector<Layer*> layer_stack_{};
  unsigned int latest_id_{0};
};

extern LayerManager* layer_manager;

class ActiveLayer {
 public:
  ActiveLayer(LayerManager& manager);
  void SetMouseLayer(unsigned int mouse_layer);
  void Activate(unsigned int layer_id);
  unsigned int GetActive() const { return active_layer_; }

 private:
  LayerManager& manager_;
  unsigned int active_layer_{0};
  unsigned int mouse_layer_{0};
};

extern ActiveLayer* active_layer;
extern std::map<unsigned int, uint64_t>* layer_task_map;

void InitializeLayer();
void ProcessLayerMessage(const Message& msg);

constexpr Message MakeLayerMessage(
    uint64_t task_id, unsigned int layer_id,
    LayerOperation op, const Rectangle<int>& area) {
  Message msg{Message::kLayer, task_id};
  msg.arg.layer.layer_id = layer_id;
  msg.arg.layer.op = op;
  msg.arg.layer.x = area.pos.x;
  msg.arg.layer.y = area.pos.y;
  msg.arg.layer.w = area.size.x;
  msg.arg.layer.h = area.size.y;
  return msg;
}

Error CloseLayer(unsigned int layer_id);
