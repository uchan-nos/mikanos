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
    HIDKeyboardDriver(Device* dev, int interface_num);
    Error Initialize() override;
    Error SetEndpoint(const EndpointConfig& config) override;
    Error OnEndpointsConfigured() override;
    Error OnControlOutCompleted(SetupData setup_data,
                                const void* buf, int len) override;
    Error OnControlInCompleted(SetupData setup_data,
                               const void* buf, int len) override;
    Error OnInterruptOutCompleted(const void* buf, int len) override;
    Error OnInterruptInCompleted(const void* buf, int len) override;

    void* operator new(size_t size);
    void operator delete(void* ptr) noexcept;

    void SubscribeKeyPush(std::function<void (uint8_t keycode)> observer);

   private:
    int ep_interrupt_in_;
    int interface_index_;

    std::array<uint8_t, 8> buf_;
    std::array<uint8_t, 8> previous_buf_;

    std::array<std::function<void (uint8_t keycode)>, 4> observers_;
    int num_observers_ = 0;

    void NotifyKeyPush(uint8_t keycode);
  };
}
