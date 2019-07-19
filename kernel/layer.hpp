#pragma once

#include <memory>
#include <list>

#include "graphics.hpp"

class Layer {
 public:
  void SetWindow(const std::shared_ptr<Window>& window) {
    window_ = window;
  }

  std::shared_ptr<Window> GetWindow() const {
    return window_;
  }

  void SetPosition(Vector2D<int> pos) {
    pos_ = pos;
  }

  void DrawTo(PixelWriter& writer) const {
    if (window_) {
      window_->DrawTo(writer, pos_);
    }
  }

 private:
  Vector2D<int> pos_;
  std::shared_ptr<Window> window_;
};

class LayerManager {
 public:
  Layer& NewLayer() {
    return layers_.emplace_back();
  }

  void DrawTo(PixelWriter& writer) const {
    for (auto& layer : layers_) {
      layer.DrawTo(writer);
    }
  }

 private:
  std::list<Layer> layers_;
};
