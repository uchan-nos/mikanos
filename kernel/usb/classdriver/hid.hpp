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
    Error OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                             const void* buf, int len) override;
    Error OnInterruptCompleted(EndpointID ep_id, const void* buf, int len) override;

    virtual Error OnDataReceived() = 0;
    const static size_t kBufferSize = 1024;
    const std::array<uint8_t, kBufferSize>& Buffer() const { return buf_; }
    const std::array<uint8_t, kBufferSize>& PreviousBuffer() const { return previous_buf_; }

   private:
    EndpointID ep_interrupt_in_;
    EndpointID ep_interrupt_out_;
    const int interface_index_;
    int in_packet_size_;
    int initialize_phase_{0};

    std::array<uint8_t, kBufferSize> buf_{}, previous_buf_{};
  };
}
