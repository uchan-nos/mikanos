#include "usb/classdriver/keyboard.hpp"

#include "usb/memory.hpp"

int printk(const char* format, ...);

namespace usb {
  Error HIDKeyboardDriver::Initialize() {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error HIDKeyboardDriver::SetEndpoint(const EndpointConfig& config) {
    printk("Device::SetEndpoint\n");
    if (config.ep_type == EndpointType::kInterrupt && config.dir_in) {
      ep_interrupt_in_ = config.ep_num;
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  void* HIDKeyboardDriver::operator new(size_t size) {
    return AllocMem(sizeof(HIDKeyboardDriver), 0, 0);
  }

  void HIDKeyboardDriver::operator delete(void* ptr) noexcept {
    FreeMem(ptr);
  }
}

