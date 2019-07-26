#include "layer.hpp"

// #@@range_begin(layer_ctor)
Layer::Layer(unsigned int id) : id_{id} {
}
// #@@range_end(layer_ctor)

// #@@range_begin(layer_id)
unsigned int Layer::ID() const {
  return id_;
}
// #@@range_end(layer_id)

// #@@range_begin(layer_setget_window)
Layer& Layer::SetWindow(const std::shared_ptr<Window>& window) {
  window_ = window;
  return *this;
}

std::shared_ptr<Window> Layer::GetWindow() const {
  return window_;
}
// #@@range_end(layer_setget_window)

// #@@range_begin(layer_move)
Layer& Layer::Move(Vector2D<int> pos) {
  pos_ = pos;
  return *this;
}

Layer& Layer::MoveRelative(Vector2D<int> pos_diff) {
  pos_ += pos_diff;
  return *this;
}
// #@@range_end(layer_move)

// #@@range_begin(layer_drawto)
void Layer::DrawTo(PixelWriter& writer) const {
  if (window_) {
    window_->DrawTo(writer, pos_);
  }
}
// #@@range_end(layer_drawto)


// #@@range_begin(layermgr_setwriter)
void LayerManager::SetWriter(PixelWriter* writer) {
  writer_ = writer;
}
// #@@range_end(layermgr_setwriter)

// #@@range_begin(layermgr_newlayer)
Layer& LayerManager::NewLayer() {
  ++latest_id_;
  auto [ it, _ ] = layers_.emplace(latest_id_, new Layer{latest_id_});
  return *it->second;
}
// #@@range_end(layermgr_newlayer)

// #@@range_begin(layermgr_draw)
void LayerManager::Draw() const {
  for (auto layer : layer_stack_) {
    layer->DrawTo(*writer_);
  }
}
// #@@range_end(layermgr_draw)

// #@@range_begin(layermgr_move)
void LayerManager::Move(unsigned int id, Vector2D<int> new_position) {
  layers_[id]->Move(new_position);
  Draw();
}

void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_diff) {
  layers_[id]->MoveRelative(pos_diff);
  Draw();
}
// #@@range_end(layermgr_move)

// #@@range_begin(layermgr_updown)
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
// #@@range_end(layermgr_updown)

// #@@range_begin(layermgr_hide)
void LayerManager::Hide(unsigned int id) {
  auto layer = layers_[id].get();
  auto pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
  if (pos != layer_stack_.end()) {
    layer_stack_.erase(pos);
  }
}
// #@@range_end(layermgr_hide)

// #@@range_begin(layermgr_topmost)
void LayerManager::Topmost(unsigned int id) {
  UpDown(id, layer_stack_.size());
}
// #@@range_end(layermgr_topmost)
