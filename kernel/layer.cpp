#include "layer.hpp"

#include <algorithm>

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
  return *layers_.emplace_back(new Layer{latest_id_});
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
  FindLayer(id)->Move(new_position);
}

void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_diff) {
  FindLayer(id)->MoveRelative(pos_diff);
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

  auto layer = FindLayer(id);
  auto old_pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
  auto new_pos = layer_stack_.begin() + new_height;

  if (old_pos == layer_stack_.end()) {
    layer_stack_.insert(new_pos, layer);
    return;
  }

  if (new_pos == layer_stack_.end()) {
    --new_pos;
  }
  layer_stack_.erase(old_pos);
  layer_stack_.insert(new_pos, layer);
}
// #@@range_end(layermgr_updown)

// #@@range_begin(layermgr_hide)
void LayerManager::Hide(unsigned int id) {
  auto layer = FindLayer(id);
  auto pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
  if (pos != layer_stack_.end()) {
    layer_stack_.erase(pos);
  }
}
// #@@range_end(layermgr_hide)

// #@@range_begin(layermgr_findlayer)
Layer* LayerManager::FindLayer(unsigned int id) {
  auto pred = [id](const std::unique_ptr<Layer>& elem) {
    return elem->ID() == id;
  };
  auto it = std::find_if(layers_.begin(), layers_.end(), pred);
  if (it == layers_.end()) {
    return nullptr;
  }
  return it->get();
}
// #@@range_end(layermgr_findlayer)

LayerManager* layer_manager;
