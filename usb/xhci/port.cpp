#include "usb/xhci/port.hpp"

#include "usb/xhci/xhci.hpp"
#include "usb/xhci/registers.hpp"

namespace usb::xhci {
  uint8_t Port::Number() const {
    return port_num_;
  }

  bool Port::IsConnected() const {
    return port_reg_set_.PORTSC.Read().bits.current_connect_status;
  }

  bool Port::IsEnabled() const {
    return port_reg_set_.PORTSC.Read().bits.port_enabled_disabled;
  }

  bool Port::IsConnectStatusChanged() const {
    return port_reg_set_.PORTSC.Read().bits.connect_status_change;
  }

  bool Port::IsPortResetChanged() const {
    return port_reg_set_.PORTSC.Read().bits.port_reset_change;
  }

  int Port::Speed() const {
    return port_reg_set_.PORTSC.Read().bits.port_speed;
  }

  Error Port::Reset() {
    auto portsc = port_reg_set_.PORTSC.Read();
    portsc.data[0] &= 0x0e00c3e0u;
    portsc.data[0] |= 0x00020010u; // Write 1 to PR and CSC
    port_reg_set_.PORTSC.Write(portsc);
    while (port_reg_set_.PORTSC.Read().bits.port_reset);
    return MAKE_ERROR(Error::kSuccess);
  }

  Device* Port::Initialize() {
    return nullptr;
  }
}
