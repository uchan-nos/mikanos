#pragma once

#include <memory>
#include <map>
#include <vector>

#include "graphics.hpp"

class Layer {
 public:
  Layer(unsigned int id = 0) : id_{id} {
  }

  unsigned int ID() const {
    return id_;
  }

  Layer& SetWindow(const std::shared_ptr<Window>& window) {
    window_ = window;
    return *this;
  }

  std::shared_ptr<Window> GetWindow() const {
    return window_;
  }

  Layer& Move(Vector2D<int> pos) {
    pos_ = pos;
    return *this;
  }

  Layer& MoveRelative(Vector2D<int> pos_diff) {
    pos_ += pos_diff;
    return *this;
  }

  void DrawTo(PixelWriter& writer) const {
    if (window_) {
      window_->DrawTo(writer, pos_);
    }
  }

 private:
  unsigned int id_;
  Vector2D<int> pos_;
  std::shared_ptr<Window> window_;
};

class LayerManager {
 public:
  void SetWriter(PixelWriter* writer) {
    writer_ = writer;
  }

  Layer& NewLayer() {
    ++latest_id_;
    auto [ it, _ ] = layers_.emplace(latest_id_, new Layer{latest_id_});
    return *it->second;
  }

  void Draw() const {
    for (auto layer : layer_stack_) {
      layer->DrawTo(*writer_);
    }
  }

  void Move(unsigned int id, Vector2D<int> new_position) {
    layers_[id]->Move(new_position);
    Draw();
  }

  void MoveRelative(unsigned int id, Vector2D<int> pos_diff) {
    layers_[id]->MoveRelative(pos_diff);
    Draw();
  }

  void UpDown(unsigned int id, int new_height) {
    if (new_height < 0) {
      Hide(id);
      return;
    }
    if (new_height > layer_stack_.size()) {
      new_height = layer_stack_.size();
    }

    auto layer = layers_[id].get();
    auto new_pos = layer_stack_.begin() + new_height;
    auto old_pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
    if (old_pos == layer_stack_.end()) {
      layer_stack_.insert(new_pos, layer);
      return;
    }
    if (old_pos == new_pos) {
      return;
    }

    if (old_pos < new_pos) {
      --new_pos;
    }
    layer_stack_.erase(old_pos);
    layer_stack_.insert(new_pos, layer);
  }

  void Hide(unsigned int id) {
    auto layer = layers_[id].get();
    auto pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
    if (pos != layer_stack_.end()) {
      layer_stack_.erase(pos);
    }
  }

  void Topmost(unsigned int id) {
    UpDown(id, layer_stack_.size());
  }

 private:
  PixelWriter* writer_{nullptr};
  std::map<unsigned int, std::unique_ptr<Layer>> layers_{};
  std::vector<Layer*> layer_stack_{};
  unsigned int latest_id_{0};
};
