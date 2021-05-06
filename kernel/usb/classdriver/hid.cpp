#include "usb/classdriver/hid.hpp"

#include <algorithm>
#include "usb/device.hpp"
#include "logger.hpp"

namespace usb {
  HIDBaseDriver::HIDBaseDriver(Device* dev, int interface_index,
                               int in_packet_size)
      : ClassDriver{dev}, interface_index_{interface_index},
        in_packet_size_{in_packet_size} {
  }

  Error HIDBaseDriver::Initialize() {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error HIDBaseDriver::SetEndpoint(const std::vector<EndpointConfig>& configs) {
    for (const auto& config : configs) {
      if (config.ep_type == EndpointType::kInterrupt && config.ep_id.IsIn()) {
        ep_interrupt_in_ = config.ep_id;
      } else if (config.ep_type == EndpointType::kInterrupt && !config.ep_id.IsIn()) {
        ep_interrupt_out_ = config.ep_id;
      }
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error HIDBaseDriver::OnEndpointsConfigured() {
    SetupData setup_data{};
    setup_data.request_type.bits.direction = request_type::kOut;
    setup_data.request_type.bits.type = request_type::kClass;
    setup_data.request_type.bits.recipient = request_type::kInterface;
    setup_data.request = request::kSetProtocol;
    setup_data.value = 0; // boot protocol
    setup_data.index = interface_index_;
    setup_data.length = 0;

    initialize_phase_ = 1;
    return ParentDevice()->ControlOut(kDefaultControlPipeID, setup_data, nullptr, 0, this);
  }

  Error HIDBaseDriver::OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                                          const void* buf, int len) {
    Log(kDebug, "HIDBaseDriver::OnControlCompleted: dev %08lx, phase = %d, len = %d\n",
        reinterpret_cast<uintptr_t>(this), initialize_phase_, len);
    if (initialize_phase_ == 1) {
      initialize_phase_ = 2;
      return ParentDevice()->NormalIn(ep_interrupt_in_, buf_.data(), in_packet_size_);
    }

    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error HIDBaseDriver::OnNormalCompleted(EndpointID ep_id, const void* buf, int len) {
    if (ep_id.IsIn()) {
      OnDataReceived();
      std::copy_n(buf_.begin(), len, previous_buf_.begin());
      return ParentDevice()->NormalIn(ep_interrupt_in_, buf_.data(), in_packet_size_);
    }

    return MAKE_ERROR(Error::kEndpointNotInCharge);
  }
}

