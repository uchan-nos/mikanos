#include "window.hpp"

#include "logger.hpp"
#include "font.hpp"

namespace {
  void DrawTextbox(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size,
                   const PixelColor& background,
                   const PixelColor& border_light,
                   const PixelColor& border_dark) {
    auto fill_rect =
      [&writer](Vector2D<int> pos, Vector2D<int> size, const PixelColor& c) {
        FillRectangle(writer, pos, size, c);
      };

    // fill main box
    fill_rect(pos + Vector2D<int>{1, 1}, size - Vector2D<int>{2, 2}, background);

    // draw border lines
    fill_rect(pos,                            {size.x, 1}, border_dark);
    fill_rect(pos,                            {1, size.y}, border_dark);
    fill_rect(pos + Vector2D<int>{0, size.y}, {size.x, 1}, border_light);
    fill_rect(pos + Vector2D<int>{size.x, 0}, {1, size.y}, border_light);
  }

  const int kCloseButtonWidth = 16;
  const int kCloseButtonHeight = 14;
  const char close_button[kCloseButtonHeight][kCloseButtonWidth + 1] = {
    "...............@",
    ".:::::::::::::$@",
    ".:::::::::::::$@",
    ".:::@@::::@@::$@",
    ".::::@@::@@:::$@",
    ".:::::@@@@::::$@",
    ".::::::@@:::::$@",
    ".:::::@@@@::::$@",
    ".::::@@::@@:::$@",
    ".:::@@::::@@::$@",
    ".:::::::::::::$@",
    ".:::::::::::::$@",
    ".$$$$$$$$$$$$$$@",
    "@@@@@@@@@@@@@@@@",
  };
}

Window::Window(int width, int height, PixelFormat shadow_format) : width_{width}, height_{height} {
  data_.resize(height);
  for (int y = 0; y < height; ++y) {
    data_[y].resize(width);
  }

  FrameBufferConfig config{};
  config.frame_buffer = nullptr;
  config.horizontal_resolution = width;
  config.vertical_resolution = height;
  config.pixel_format = shadow_format;

  if (auto err = shadow_buffer_.Initialize(config)) {
    Log(kError, "failed to initialize shadow buffer: %s at %s:%d\n",
        err.Name(), err.File(), err.Line());
  }
}

void Window::DrawTo(FrameBuffer& dst, Vector2D<int> pos, const Rectangle<int>& area) {
  if (!transparent_color_) {
    Rectangle<int> window_area{pos, Size()};
    Rectangle<int> intersection = area & window_area;
    dst.Copy(intersection.pos, shadow_buffer_, {intersection.pos - pos, intersection.size});
    return;
  }

  const auto tc = transparent_color_.value();
  auto& writer = dst.Writer();
  for (int y = std::max(0, 0 - pos.y);
       y < std::min(Height(), writer.Height() - pos.y);
       ++y) {
    for (int x = std::max(0, 0 - pos.x);
         x < std::min(Width(), writer.Width() - pos.x);
         ++x) {
      const auto c = At(Vector2D<int>{x, y});
      if (c != tc) {
        writer.Write(pos + Vector2D<int>{x, y}, c);
      }
    }
  }
}

void Window::SetTransparentColor(std::optional<PixelColor> c) {
  transparent_color_ = c;
}

Window::WindowWriter* Window::Writer() {
  return &writer_;
}

const PixelColor& Window::At(Vector2D<int> pos) const{
  return data_[pos.y][pos.x];
}

void Window::Write(Vector2D<int> pos, PixelColor c) {
  data_[pos.y][pos.x] = c;
  shadow_buffer_.Writer().Write(pos, c);
}

int Window::Width() const {
  return width_;
}

int Window::Height() const {
  return height_;
}

Vector2D<int> Window::Size() const {
  return {width_, height_};
}

void Window::Move(Vector2D<int> dst_pos, const Rectangle<int>& src) {
  shadow_buffer_.Move(dst_pos, src);
}

WindowRegion Window::GetWindowRegion(Vector2D<int> pos) {
  return WindowRegion::kOther;
}

ToplevelWindow::ToplevelWindow(int width, int height, PixelFormat shadow_format,
                               const std::string& title)
    : Window{width, height, shadow_format}, title_{title} {
  DrawWindow(*Writer(), title_.c_str());
}

void ToplevelWindow::Activate() {
  Window::Activate();
  DrawWindowTitle(*Writer(), title_.c_str(), true);
}

void ToplevelWindow::Deactivate() {
  Window::Deactivate();
  DrawWindowTitle(*Writer(), title_.c_str(), false);
}

WindowRegion ToplevelWindow::GetWindowRegion(Vector2D<int> pos) {
  if (pos.x < 2 || Width() - 2 <= pos.x ||
      pos.y < 2 || Height() - 2 <= pos.y) {
    return WindowRegion::kBorder;
  } else if (pos.y < kTopLeftMargin.y) {
    if (Width() - 5 - kCloseButtonWidth <= pos.x && pos.x < Width() - 5 &&
        5 <= pos.y && pos.y < 5 + kCloseButtonHeight) {
      return WindowRegion::kCloseButton;
    }
    return WindowRegion::kTitleBar;
  }
  return WindowRegion::kOther;
}

Vector2D<int> ToplevelWindow::InnerSize() const {
  return Size() - kTopLeftMargin - kBottomRightMargin;
}

void DrawWindow(PixelWriter& writer, const char* title) {
  auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, uint32_t c) {
    FillRectangle(writer, pos, size, ToColor(c));
  };
  const auto win_w = writer.Width();
  const auto win_h = writer.Height();

  fill_rect({0, 0},         {win_w, 1},             0xc6c6c6);
  fill_rect({1, 1},         {win_w - 2, 1},         0xffffff);
  fill_rect({0, 0},         {1, win_h},             0xc6c6c6);
  fill_rect({1, 1},         {1, win_h - 2},         0xffffff);
  fill_rect({win_w - 2, 1}, {1, win_h - 2},         0x848484);
  fill_rect({win_w - 1, 0}, {1, win_h},             0x000000);
  fill_rect({2, 2},         {win_w - 4, win_h - 4}, 0xc6c6c6);
  fill_rect({1, win_h - 2}, {win_w - 2, 1},         0x848484);
  fill_rect({0, win_h - 1}, {win_w, 1},             0x000000);

  DrawWindowTitle(writer, title, false);
}

void DrawTextbox(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size) {
  DrawTextbox(writer, pos, size,
              ToColor(0xffffff), ToColor(0xc6c6c6), ToColor(0x848484));
}

void DrawTerminal(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size) {
  DrawTextbox(writer, pos, size,
              ToColor(0x000000), ToColor(0xc6c6c6), ToColor(0x848484));
}

void DrawWindowTitle(PixelWriter& writer, const char* title, bool active) {
  const auto win_w = writer.Width();
  uint32_t bgcolor = 0x848484;
  if (active) {
    bgcolor = 0x000084;
  }

  FillRectangle(writer, {3, 3}, {win_w - 6, 18}, ToColor(bgcolor));
  WriteString(writer, {24, 4}, title, ToColor(0xffffff));

  for (int y = 0; y < kCloseButtonHeight; ++y) {
    for (int x = 0; x < kCloseButtonWidth; ++x) {
      PixelColor c = ToColor(0xffffff);
      if (close_button[y][x] == '@') {
        c = ToColor(0x000000);
      } else if (close_button[y][x] == '$') {
        c = ToColor(0x848484);
      } else if (close_button[y][x] == ':') {
        c = ToColor(0xc6c6c6);
      }
      writer.Write({win_w - 5 - kCloseButtonWidth + x, 5 + y}, c);
    }
  }
}
