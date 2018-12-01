/**
 * @file usb/classdriver/keyboard.hpp
 *
 * HID keyboard class driver.
 */

#pragma once

#include "usb/classdriver/hid.hpp"

namespace usb {
  class HIDKeyboardDriver : public HIDBaseDriver {
   public:
    HIDKeyboardDriver(Device* dev, int interface_index);

    void* operator new(size_t size);
    void operator delete(void* ptr) noexcept;

    Error OnDataReceived() override;

    void SubscribeKeyPush(std::function<void (uint8_t keycode)> observer);

   private:
    std::array<std::function<void (uint8_t keycode)>, 4> observers_;
    int num_observers_ = 0;

    void NotifyKeyPush(uint8_t keycode);
  };
}
