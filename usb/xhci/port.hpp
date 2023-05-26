/**
 * @file usb/xhci/port.hpp
 *
 * xHCI の各ポートを表すクラスと周辺機能．
 */

#pragma once

#include <cstdint>
#include "error.hpp"

#include "usb/xhci/registers.hpp"

#define CLEAR_STATUS_BIT(bitname) \
  [this](){ \
    PORTSC_Bitmap portsc = port_reg_set_.PORTSC.Read(); \
    portsc.data[0] &= 0x0e01c3e0u; \
    portsc.bits.bitname = 1; \
    port_reg_set_.PORTSC.Write(portsc); \
  }()

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
    bool IsPortResetChanged() const;
    int Speed() const;
    Error Reset();
    Device* Initialize();

    void ClearConnectStatusChanged() const {
      CLEAR_STATUS_BIT(connect_status_change);
    }
    void ClearPortResetChange() const {
      CLEAR_STATUS_BIT(port_reset_change);
    }

   private:
    const uint8_t port_num_;
    PortRegisterSet& port_reg_set_;
  };
}
