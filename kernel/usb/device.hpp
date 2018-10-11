/**
 * @file usb/device.hpp
 *
 * USB デバイスを表すクラスと関連機能．
 */

#pragma once

#include "error.hpp"

namespace usb {
  class Device {
   public:
    virtual Error ControlIn(int ep_num, uint64_t setup_data,
                            void* buf, int len) = 0;
    virtual Error ControlOut(int ep_num, uint64_t setup_data,
                             const void* buf, int len) = 0;
  };

  Error GetDescriptor(Device& dev, int ep_num,
                      uint8_t desc_type, uint8_t desc_index,
                      void* buf, int len, bool debug = false);
  Error ConfigureEndpoints(Device& dev, bool debug = false);
}
