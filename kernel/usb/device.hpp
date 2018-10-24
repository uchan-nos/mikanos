/**
 * @file usb/device.hpp
 *
 * USB デバイスを表すクラスと関連機能．
 */

#pragma once

#include <array>

#include "error.hpp"

namespace usb {
  class Device {
   public:
    virtual ~Device();
    virtual Error ControlIn(int ep_num, uint64_t setup_data,
                            void* buf, int len) = 0;
    virtual Error ControlOut(int ep_num, uint64_t setup_data,
                             const void* buf, int len) = 0;

    Error StartInitialize();
    Error OnControlOutCompleted(const void* buf, size_t len);
    Error OnControlInCompleted(const void* buf, size_t len);
    bool IsInitialized() { return is_initialized_; }

    uint8_t* Buffer() { return buf_.data(); }

   private:
    std::array<uint8_t, 256> buf_{};
    bool is_initialized_ = false;

    // following fields are used during initialization
    uint8_t num_configurations_;
    uint8_t config_index_;

    Error OnDeviceDescriptorReceived(const uint8_t* buf, size_t len);
    Error OnConfigurationDescriptorReceived(const uint8_t* buf, size_t len);
  };

  Error GetDescriptor(Device& dev, int ep_num,
                      uint8_t desc_type, uint8_t desc_index,
                      void* buf, int len, bool debug = false);
  Error ConfigureEndpoints(Device& dev, bool debug = false);
}
