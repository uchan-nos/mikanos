#include "usb/classdriver/keyboard.hpp"

#include <algorithm>
#include "usb/memory.hpp"
#include "usb/device.hpp"

namespace usb {
  HIDKeyboardDriver::HIDKeyboardDriver(Device* dev, int interface_index)
      : HIDBaseDriver{dev, interface_index, 8} {
  }

  Error HIDKeyboardDriver::OnDataReceived() {
    for (int i = 2; i < 8; ++i) {
      const uint8_t key = Buffer()[i];
      if (key == 0) {
        continue;
      }
      const auto& prev_buf = PreviousBuffer();
      if (std::find(prev_buf.begin(), prev_buf.end(), key) != prev_buf.end()) {
        continue;
      }
      NotifyKeyPush(key);
    }
    return MAKE_ERROR(Error::kSuccess);
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

  std::function<HIDKeyboardDriver::ObserverType> HIDKeyboardDriver::default_observer;

  void HIDKeyboardDriver::NotifyKeyPush(uint8_t keycode) {
    for (int i = 0; i < num_observers_; ++i) {
      observers_[i](keycode);
    }
  }
}

