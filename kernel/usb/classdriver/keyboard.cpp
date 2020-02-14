#include "usb/classdriver/keyboard.hpp"

#include <algorithm>
#include <bitset>
#include "usb/memory.hpp"
#include "usb/device.hpp"

namespace usb {
  HIDKeyboardDriver::HIDKeyboardDriver(Device* dev, int interface_index)
      : HIDBaseDriver{dev, interface_index, 8} {
  }

  Error HIDKeyboardDriver::OnDataReceived() {
    std::bitset<256> prev, current;
    for (int i = 2; i < 8; ++i) {
      prev.set(PreviousBuffer()[i], true);
      current.set(Buffer()[i], true);
    }
    const auto changed = prev ^ current;
    const auto pressed = changed & current;
    for (int key = 1; key < 256; ++key) {
      if (changed.test(key)) {
        NotifyKeyPush(Buffer()[0], key, pressed.test(key));
      }
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
      std::function<ObserverType> observer) {
    observers_[num_observers_++] = observer;
  }

  std::function<HIDKeyboardDriver::ObserverType> HIDKeyboardDriver::default_observer;

  void HIDKeyboardDriver::NotifyKeyPush(uint8_t modifier, uint8_t keycode, bool press) {
    for (int i = 0; i < num_observers_; ++i) {
      observers_[i](modifier, keycode, press);
    }
  }
}
