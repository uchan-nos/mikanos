/**
 * @file usb/classdriver/hid.hpp
 *
 * HID base class driver.
 */

#pragma once

#include "usb/classdriver/base.hpp"

namespace usb {
  class HIDBaseDriver : public ClassDriver {
   public:
    HIDBaseDriver(Device* dev, int interface_index, int in_packet_size);
    Error Initialize() override;
    Error SetEndpoint(const EndpointConfig& config) override;
    Error OnEndpointsConfigured() override;
    Error OnControlOutCompleted(SetupData setup_data,
                                const void* buf, int len) override;
    Error OnControlInCompleted(SetupData setup_data,
                               const void* buf, int len) override;
    Error OnInterruptOutCompleted(const void* buf, int len) override;
    Error OnInterruptInCompleted(const void* buf, int len) override;

    virtual Error OnDataReceived() = 0;
    const std::array<uint8_t, 8>& Buffer() const { return buf_; }
    const std::array<uint8_t, 8>& PreviousBuffer() const { return previous_buf_; }

   private:
    int ep_interrupt_in_;
    int ep_interrupt_out_;
    const int interface_index_;
    int in_packet_size_;

    std::array<uint8_t, 8> buf_{}, previous_buf_{};
  };
}
