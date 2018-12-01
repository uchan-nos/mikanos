#include "usb/classdriver/hid.hpp"

#include <algorithm>
#include "usb/device.hpp"

int printk(const char* format, ...);

namespace usb {
  HIDBaseDriver::HIDBaseDriver(Device* dev, int interface_index,
                               int in_packet_size)
      : ClassDriver{dev}, interface_index_{interface_index},
        in_packet_size_{in_packet_size} {
  }

  Error HIDBaseDriver::Initialize() {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error HIDBaseDriver::SetEndpoint(const EndpointConfig& config) {
    if (config.ep_type == EndpointType::kInterrupt && config.dir_in) {
      ep_interrupt_in_ = config.ep_num;
    } else if (config.ep_type == EndpointType::kInterrupt && !config.dir_in) {
      ep_interrupt_out_ = config.ep_num;
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error HIDBaseDriver::OnEndpointsConfigured() {
    SetupData setup_data{};
    setup_data.request_type.bits.direction = request_type::kIn;
    setup_data.request_type.bits.type = request_type::kClass;
    setup_data.request_type.bits.recipient = request_type::kInterface;
    setup_data.request = request::kGetReport;
    setup_data.value = 0x0100; // Input Report
    setup_data.index = interface_index_;
    setup_data.length = in_packet_size_; // bytes
    return ParentDevice()->ControlIn(0, setup_data, buf_.data(), buf_.size());
  }

  Error HIDBaseDriver::OnControlOutCompleted(SetupData setup_data,
                                                 const void* buf, int len) {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error HIDBaseDriver::OnControlInCompleted(SetupData setup_data,
                                                const void* buf, int len) {
    if (buf != buf_.data()) {
      return MAKE_ERROR(Error::kSuccess);
    }
    printk("HIDBaseDriver::OnControlInCompleted: len = %d\n", len);
    OnDataReceived();
    std::copy_n(buf_.begin(), len, previous_buf_.begin());
    return ParentDevice()->InterruptIn(ep_interrupt_in_, buf_.data(), buf_.size());
  }

  Error HIDBaseDriver::OnInterruptOutCompleted(const void* buf, int len) {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error HIDBaseDriver::OnInterruptInCompleted(const void* buf, int len) {
    if (buf != buf_.data()) {
      return MAKE_ERROR(Error::kSuccess);
    }
    printk("HIDBaseDriver::OnInterruptInCompleted: len = %d\n", len);
    OnDataReceived();
    std::copy_n(buf_.begin(), len, previous_buf_.begin());
    return ParentDevice()->InterruptIn(ep_interrupt_in_, buf_.data(), buf_.size());
  }
}

