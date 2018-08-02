#include <cstdint>
#include <cstddef>

void* operator new(size_t size, void* buf)
{
  return buf;
}

void operator delete(void* obj) noexcept
{
}

class PixelWriter {
 public:
  PixelWriter(uint8_t* frame_buffer, unsigned int pixels_per_line,
              unsigned int bytes_per_pixel)
      : frame_buffer_{frame_buffer}, pixels_per_line_{pixels_per_line},
        bytes_per_pixel_{bytes_per_pixel} {
  }
  virtual ~PixelWriter() {}
  virtual void Write(int x, int y, uint8_t r, uint8_t g, uint8_t b) = 0;

 protected:
  uint8_t* PixelAt(int x, int y) {
    return frame_buffer_ + bytes_per_pixel_ * (pixels_per_line_ * y + x);
  }
 private:
  uint8_t* const frame_buffer_;
  const unsigned int pixels_per_line_, bytes_per_pixel_;
};

class RGBReserved8BitPerColorPixelWriter : public PixelWriter {
 public:
  using PixelWriter::PixelWriter;
  virtual void Write(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    auto p = PixelAt(x, y);
    p[0] = r;
    p[1] = g;
    p[2] = b;
  }
};

void WritePixel(uint8_t* frame_buffer, unsigned int pixels_per_line,
                int pixel_format, int x, int y,
                uint8_t r, uint8_t g, uint8_t b) {
  if (pixel_format == 0) {
    auto p = &frame_buffer[4 * (pixels_per_line * y + x)];
    p[0] = r;
    p[1] = g;
    p[2] = b;
  }
}

extern "C" void KernelMain(uint64_t frame_buffer_base,
                           uint64_t frame_buffer_size) {
  uint8_t* frame_buffer = reinterpret_cast<uint8_t*>(frame_buffer_base);
  //for (uint64_t i = 0; i < frame_buffer_size; ++i) {
  //  frame_buffer[i] = i % 256;
  //}

  uint8_t pixel_writer_buffer[sizeof(RGBReserved8BitPerColorPixelWriter) + 32];
  PixelWriter* pixel_writer;
  pixel_writer = new(pixel_writer_buffer) RGBReserved8BitPerColorPixelWriter(
      frame_buffer, 800, 4);
  for (int x = 0; x < 50; ++x) {
    for (int y = 0; y < 30; ++y) {
      //pixel_writer->Write(100 + x, 100 + y, 0, 255, 0);
      //pixel_writer->Write(x, y, x + y, 0, 0);
      WritePixel(frame_buffer, 800, 0, 100 + x, 100 + y, 0, 0, 255);
    }
  }
  while (1) __asm__("hlt");
}
