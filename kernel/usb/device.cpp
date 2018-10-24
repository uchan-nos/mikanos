#include "usb/device.hpp"

#include "usb/descriptor.hpp"
#include "usb/setupdata.hpp"

int printk(const char* format, ...);

namespace usb {
  Device::~Device() {
  }

  Error Device::StartInitialize() {
    is_initialized_ = false;
    return GetDescriptor(*this, 0, DeviceDescriptor::kType, 0,
                         buf_.data(), buf_.size(), true);
  }

  Error Device::OnControlOutCompleted(const void* buf, size_t len) {
    printk("OnControlOutCompleted: %p, %d\n", buf, len);
    return Error::kSuccess;
  }

  Error Device::OnControlInCompleted(const void* buf, size_t len) {
    printk("OnControlInCompleted: %p, %d\n", buf, len);
    return Error::kSuccess;
  }

  Error Device::OnDeviceDescriptorReceived(const uint8_t* desc_data, size_t len) {
    return Error::kNotImplemented;
  }

  Error Device::OnConfigurationDescriptorReceived(const uint8_t* desc_data, size_t len) {
    return Error::kNotImplemented;
  }

  Error GetDescriptor(Device& dev, int ep_num,
                      uint8_t desc_type, uint8_t desc_index,
                      void* buf, int len, bool debug) {
    SetupData setup_data{};
    setup_data.bits.direction = request_type::kIn;
    setup_data.bits.type = request_type::kStandard;
    setup_data.bits.recipient = request_type::kDevice;
    setup_data.bits.request = request::kGetDescriptor;
    setup_data.bits.value = (static_cast<uint16_t>(desc_type) << 8) | desc_index;
    setup_data.bits.index = 0;
    setup_data.bits.length = len;
    return dev.ControlIn(ep_num, setup_data.data, buf, len);
  }

  /*
  Error ConfigureEndpoints(Device& dev, bool debug) {
    if (debug) printk("configuring endpoints\n");

    initialize_phase_ = 1;

    memset(desc_buf_, 0, sizeof(desc_buf_));
    if (auto err = GetDescriptor(descriptor_type::kDevice, 0); IsError(err))
    {
      return err;
    }

    return error::kSuccess;
  }
  */
}
