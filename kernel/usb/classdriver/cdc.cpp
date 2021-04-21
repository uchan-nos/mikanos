#include "usb/classdriver/cdc.hpp"

#include <cstdlib>

#include "logger.hpp"
#include "usb/device.hpp"

namespace usb::cdc {
  CDCDriver::CDCDriver(Device* dev, const InterfaceDescriptor* if_comm,
                       const InterfaceDescriptor* if_data) : ClassDriver{dev} {
  }

  Error CDCDriver::Initialize() {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error CDCDriver::SetEndpoint(const EndpointConfig& config) {
    if (config.ep_type == EndpointType::kInterrupt && config.ep_id.IsIn()) {
      ep_interrupt_in_ = config.ep_id;
    } else if (config.ep_type == EndpointType::kBulk && config.ep_id.IsIn()) {
      ep_bulk_in_ = config.ep_id;
    } else if (config.ep_type == EndpointType::kBulk && !config.ep_id.IsIn()) {
      ep_bulk_out_ = config.ep_id;
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error CDCDriver::OnEndpointsConfigured() {
    return MAKE_ERROR(Error::kSuccess);
  }

  Error CDCDriver::OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                           const void* buf, int len) {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error CDCDriver::OnInterruptCompleted(EndpointID ep_id, const void* buf, int len) {
    Log(kWarn, "cdc intr compl: buf='%.*s'\n", len, buf);
    return MAKE_ERROR(Error::kSuccess);
  }

  Error CDCDriver::SendSerial(const void* buf, int len) {
    uint8_t* buf_out = new uint8_t[len];
    memcpy(buf_out, buf, len);
    if (auto err = ParentDevice()->NormalOut(ep_bulk_out_, buf_out, len)) {
      Log(kError, "%s:%d: NormalOut failed: %s\n", err.File(), err.Line(), err.Name());
      return err;
    }

    uint8_t* buf_in = new uint8_t[8];
    if (auto err = ParentDevice()->NormalIn(ep_bulk_in_, buf_in, 8)) {
      Log(kError, "%s:%d: NormalIn failed: %s\n", err.File(), err.Line(), err.Name());
      return err;
    }
    return MAKE_ERROR(Error::kSuccess);
  }
}
