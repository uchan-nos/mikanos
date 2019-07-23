#include "layer.hpp"

Layer::Layer(unsigned int id) : id_{id} {
}

unsigned int Layer::ID() const {
  return id_;
}

Layer& Layer::SetWindow(const std::shared_ptr<Window>& window) {
  window_ = window;
  return *this;
}

std::shared_ptr<Window> Layer::GetWindow() const {
  return window_;
}

Layer& Layer::Move(Vector2D<int> pos) {
  pos_ = pos;
  return *this;
}

Layer& Layer::MoveRelative(Vector2D<int> pos_diff) {
  pos_ += pos_diff;
  return *this;
}

void Layer::DrawTo(PixelWriter& writer) const {
  if (window_) {
    window_->DrawTo(writer, pos_);
  }
}


void LayerManager::SetWriter(PixelWriter* writer) {
  writer_ = writer;
}

Layer& LayerManager::NewLayer() {
  ++latest_id_;
  auto [ it, _ ] = layers_.emplace(latest_id_, new Layer{latest_id_});
  return *it->second;
}

void LayerManager::Draw() const {
  for (auto layer : layer_stack_) {
    layer->DrawTo(*writer_);
  }
}

void LayerManager::Move(unsigned int id, Vector2D<int> new_position) {
  layers_[id]->Move(new_position);
  Draw();
}

void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_diff) {
  layers_[id]->MoveRelative(pos_diff);
  Draw();
}

void LayerManager::UpDown(unsigned int id, int new_height) {
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

void LayerManager::Hide(unsigned int id) {
  auto layer = layers_[id].get();
  auto pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
  if (pos != layer_stack_.end()) {
    layer_stack_.erase(pos);
  }
}

void LayerManager::Topmost(unsigned int id) {
  UpDown(id, layer_stack_.size());
}
