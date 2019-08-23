/**
 * @file usb/classdriver/mouse.hpp
 *
 * HID mouse class driver.
 */

#pragma once

#include <functional>
#include "usb/classdriver/hid.hpp"

namespace usb {
  class HIDMouseDriver : public HIDBaseDriver {
   public:
    HIDMouseDriver(Device* dev, int interface_index);

    void* operator new(size_t size);
    void operator delete(void* ptr) noexcept;

    Error OnDataReceived() override;

    using ObserverType = void (uint8_t buttons, int8_t displacement_x, int8_t displacement_y);
    void SubscribeMouseMove(std::function<ObserverType> observer);
    static std::function<ObserverType> default_observer;

   private:
    std::array<std::function<ObserverType>, 4> observers_;
    int num_observers_ = 0;

    void NotifyMouseMove(uint8_t buttons, int8_t displacement_x, int8_t displacement_y);
  };
}
