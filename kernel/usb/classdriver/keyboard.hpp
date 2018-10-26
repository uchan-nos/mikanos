/**
 * @file usb/classdriver/keyboard.hpp
 *
 * HID keyboard class driver.
 */

#pragma once

#include "usb/classdriver/base.hpp"

namespace usb {
  class HIDKeyboardDriver : public ClassDriver {
   public:
    Error Initialize() override;
    Error SetEndpoint(const EndpointConfig& config) override;

    void* operator new(size_t size);
    void operator delete(void* ptr) noexcept;

   private:
    int ep_interrupt_in_;
  };
}
