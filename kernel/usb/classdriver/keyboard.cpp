#include "usb/classdriver/keyboard.hpp"

#include "usb/memory.hpp"
#include "usb/device.hpp"

int printk(const char* format, ...);

namespace usb {
  HIDKeyboardDriver::HIDKeyboardDriver(Device* dev, int interface_index)
      : ClassDriver{dev}, interface_index_{interface_index} {
  }

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

  Error HIDKeyboardDriver::OnEndpointsConfigured() {
    SetupData setup_data{};
    setup_data.request_type.bits.direction = request_type::kIn;
    setup_data.request_type.bits.type = request_type::kClass;
    setup_data.request_type.bits.recipient = request_type::kInterface;
    setup_data.request = request::kGetReport;
    setup_data.value = 0x0100; // Input Report
    setup_data.index = interface_index_;
    setup_data.length = 8; // bytes
    return ParentDevice()->ControlIn(0, setup_data, buf_.data(), buf_.size());
  }

  Error HIDKeyboardDriver::OnControlOutCompleted(SetupData setup_data,
                                                 const void* buf, int len) {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error HIDKeyboardDriver::OnControlInCompleted(SetupData setup_data,
                                                const void* buf, int len) {
    if (buf != buf_.data()) {
      return MAKE_ERROR(Error::kSuccess);
    }
    printk("HIDKeyboardDriver::OnControlInCompleted: len = %d\n", len);
    std::copy_n(buf, len, previous_buf_.begin());
    return MAKE_ERROR(Error::kSuccess);
  }

  void* HIDKeyboardDriver::operator new(size_t size) {
    return AllocMem(sizeof(HIDKeyboardDriver), 0, 0);
  }

  void HIDKeyboardDriver::operator delete(void* ptr) noexcept {
    FreeMem(ptr);
  }
}

