/**
 * @file usb/classdriver/mouse.hpp
 *
 * HID mouse class driver.
 */

#pragma once

#include "usb/classdriver/hid.hpp"

namespace usb {
  class HIDMouseDriver : public HIDBaseDriver {
   public:
    HIDMouseDriver(Device* dev, int interface_index);

    void* operator new(size_t size);
    void operator delete(void* ptr) noexcept;

    Error OnDataReceived() override;

    void SubscribeMouseMove(std::function<void (int8_t displacement_x, int8_t displacement_y)> observer);

   private:
    std::array<std::function<void (int8_t displacement_x, int8_t displacement_y)>, 4> observers_;
    int num_observers_ = 0;

    void NotifyMouseMove(int8_t displacement_x, int8_t displacement_y);
  };
}
