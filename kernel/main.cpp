/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル．
 */

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <numeric>
#include <vector>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "mouse.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/keyboard.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

void operator delete(void* obj) noexcept {
}

const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

char console_buf[sizeof(Console)];
Console* console;

int printk(const char* format, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  console->PutString(s);
  return result;
}

const char keycode_map[256] = {
  0,    0,    0,    0,  'a',  'b',  'c',  'd', // 0
  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l', // 8
  'm',  'n',  'o',  'p',  'q',  'r',  's',  't', // 16
  'u',  'v',  'w',  'x',  'y',  'z',  '1',  '2', // 24
  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0', // 32
  '\n', '\b', 0x08, '\t',  ' ',  '-',  '=',  '[', // 40
  ']', '\\',  '#',  ';', '\'',  '`',  ',',  '.', // 48
  '/',    0,    0,    0,    0,    0,    0,    0, // 56
  0,    0,    0,    0,    0,    0,    0,    0, // 64
  0,    0,    0,    0,    0,    0,    0,    0, // 72
  0,    0,    0,    0,  '/',  '*',  '-',  '+', // 80
  '\n',  '1',  '2',  '3',  '4',  '5',  '6',  '7', // 88
  '8',  '9',  '0',  '.', '\\',    0,    0,  '=', // 96
};

const char keycode_map_shifted[256] = {
  0,    0,    0,    0,  'A',  'B',  'C',  'D', // 0
  'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L', // 8
  'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T', // 16
  'U',  'V',  'W',  'X',  'Y',  'Z',  '!',  '@', // 24
  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')', // 32
  '\n', '\b', 0x08, '\t',  ' ',  '_',  '+',  '{', // 40
  '}',  '|',  '~',  ':',  '"',  '~',  '<',  '>', // 48
  '?',    0,    0,    0,    0,    0,    0,    0, // 56
  0,    0,    0,    0,    0,    0,    0,    0, // 64
  0,    0,    0,    0,    0,    0,    0,    0, // 72
  0,    0,    0,    0,  '/',  '*',  '-',  '+', // 80
  '\n',  '1',  '2',  '3',  '4',  '5',  '6',  '7', // 88
  '8',  '9',  '0',  '.', '\\',    0,    0,  '=', // 96
};

void KeyboardObserver(uint8_t keycode) {
  const char ascii = keycode_map[keycode];
  if (ascii != '\0') {
    printk("%c", ascii);
  }
}

char mouse_cursor_buf[sizeof(MouseCursor)];
MouseCursor* mouse_cursor;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
  mouse_cursor->MoveRelative({displacement_x, displacement_y});
}

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
  switch (frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf)
        RGBResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
    case kPixelBGRResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf)
        BGRResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
  }

  const int kFrameWidth = frame_buffer_config.horizontal_resolution;
  const int kFrameHeight = frame_buffer_config.vertical_resolution;

  FillRectangle(*pixel_writer,
                {0, 0},
                {kFrameWidth, kFrameHeight - 50},
                kDesktopBGColor);
  FillRectangle(*pixel_writer,
                {0, kFrameHeight - 50},
                {kFrameWidth, 50},
                {1, 8, 17});
  FillRectangle(*pixel_writer,
                {0, kFrameHeight - 50},
                {kFrameWidth / 5, 50},
                {80, 80, 80});
  DrawRectangle(*pixel_writer,
                {10, kFrameHeight - 40},
                {30, 30},
                {160, 160, 160});

  console = new(console_buf) Console{
    *pixel_writer, kDesktopFGColor, kDesktopBGColor
  };
  printk("Welcome to MikanOS!\n");
  SetLogLevel(kWarn);

  mouse_cursor = new(mouse_cursor_buf) MouseCursor{
    pixel_writer, kDesktopBGColor, {300, 200}
  };

  auto err = pci::ScanAllBus();
  Log(kDebug, "ScanAllBus: %s\n", err.Name());

  for (int i = 0; i < pci::num_device; ++i) {
    const auto& dev = pci::devices[i];
    auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
    auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
    Log(kDebug, "%d.%d.%d: vend %04x, class %08x, head %02x\n",
        dev.bus, dev.device, dev.function,
        vendor_id, class_code, dev.header_type);
  }

  // #@@range_begin(find_xhc)
  pci::Device* xhc_dev = nullptr;
  for (int i = 0; i < pci::num_device; ++i) {
    if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)) {
      // xHC
      xhc_dev = &pci::devices[i];
      break;
    }
  }

  if (xhc_dev) {
    Log(kInfo, "xHC has been found: %d.%d.%d\n",
        xhc_dev->bus, xhc_dev->device, xhc_dev->function);
  }
  // #@@range_end(find_xhc)

  const auto bar = pci::ReadBar(*xhc_dev, 0);
  //const auto mmio_base = bitutil::ClearBits(bar.value, 0xf);
  const auto mmio_base = bar.value & ~static_cast<uint64_t>(0xf);
  Log(kDebug, "xHC mmio_base = %08lx\n", mmio_base);
  usb::xhci::Controller xhc{mmio_base};

  {
    auto err = xhc.Initialize();
    Log(kDebug, "xhc.Initialize: %s\n", err.Name());
  }

  Log(kInfo, "xHC starting\n");
  xhc.Run();

  usb::HIDKeyboardDriver::default_observer = KeyboardObserver;
  usb::HIDMouseDriver::default_observer = MouseObserver;

  for (int i = 1; i <= xhc.MaxPorts(); ++i) {
    auto port = xhc.PortAt(i);
    Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());

    if (port.IsConnected()) {
      if (auto err = ConfigurePort(xhc, port)) {
        Log(kError, "failed to configure port: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
        continue;
      }
    }
  }

  while (1) {
    if (auto err = ProcessEvent(xhc)) {
      Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
          err.Name(), err.File(), err.Line());
    }
  }

  while (1) __asm__("hlt");
}

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}
