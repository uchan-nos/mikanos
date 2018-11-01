#include "usb/classdriver/keyboard.hpp"

#include <algorithm>
#include <functional>
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
    std::copy_n(static_cast<const uint8_t*>(buf), len, previous_buf_.begin());
    for (int i = 2; i < 8; ++i) {
      if (previous_buf_[i] != 0) {
        NotifyKeyPush(previous_buf_[i]);
      }
    }
    return ParentDevice()->InterruptIn(ep_interrupt_in_, buf_.data(), buf_.size());
  }

  Error HIDKeyboardDriver::OnInterruptOutCompleted(const void* buf, int len) {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error HIDKeyboardDriver::OnInterruptInCompleted(const void* buf, int len) {
    if (buf != buf_.data()) {
      return MAKE_ERROR(Error::kSuccess);
    }
    printk("HIDKeyboardDriver::OnInterruptInCompleted: len = %d\n", len);
    auto buf8 = static_cast<const uint8_t*>(buf);
    for (int i = 2; i < 8; ++i) {
      if (buf8[i] == 0) {
        continue;
      }
      for (int j = 2; j < 8; ++j) {
        if (buf8[i] == previous_buf_[j]) {
          continue;
        }
      }
      NotifyKeyPush(buf8[i]);
    }
    std::copy_n(buf8, len, previous_buf_.begin());
    return ParentDevice()->InterruptIn(ep_interrupt_in_, buf_.data(), buf_.size());
  }

  void* HIDKeyboardDriver::operator new(size_t size) {
    return AllocMem(sizeof(HIDKeyboardDriver), 0, 0);
  }

  void HIDKeyboardDriver::operator delete(void* ptr) noexcept {
    FreeMem(ptr);
  }

  void HIDKeyboardDriver::SubscribeKeyPush(
      std::function<void (uint8_t keycode)> observer) {
    observers_[num_observers_++] = observer;
  }

  void HIDKeyboardDriver::NotifyKeyPush(uint8_t keycode) {
    for (int i = 0; i < num_observers_; ++i) {
      observers_[i](keycode);
    }
  }
}

