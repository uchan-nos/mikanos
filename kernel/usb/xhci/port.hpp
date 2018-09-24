/**
 * @file usb/xhci/port.hpp
 *
 * xHCI の各ポートを表すクラスと周辺機能．
 */

#pragma once

#include <cstdint>
#include "error.hpp"

namespace usb::xhci {
  class Controller;
  struct PortRegisterSet;
  class Device;

  class Port {
   public:
    Port(uint8_t port_num, PortRegisterSet& port_reg_set)
      : port_num_{port_num}, port_reg_set_{port_reg_set}
    {}

    uint8_t Number() const;
    bool IsConnected() const;
    bool IsEnabled() const;
    bool IsConnectStatusChanged() const;
    int Speed() const;
    Error Reset();
    Device* Initialize();

   private:
    const uint8_t port_num_;
    PortRegisterSet& port_reg_set_;
  };
}
