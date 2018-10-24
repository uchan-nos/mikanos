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
    return Error::kNotImplemented;
  }

  Error Device::OnControlInCompleted(const void* buf, size_t len) {
    const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(buf);
    if (auto device_desc = DescriptorDynamicCast<DeviceDescriptor>(buf8)) {
      return OnDeviceDescriptorReceived(buf8, len);
    } else if (auto config_desc = DescriptorDynamicCast<ConfigurationDescriptor>(buf8)) {
      return OnConfigurationDescriptorReceived(buf8, len);
    } else {
      return Error::kNotImplemented;
    }
  }

  Error Device::OnDeviceDescriptorReceived(const uint8_t* buf, size_t len) {
    if (is_initialized_) {
      return Error::kSuccess;
    }

    const auto device_desc = DescriptorDynamicCast<DeviceDescriptor>(buf);
    num_configurations_ = device_desc->num_configurations;
    config_index_ = 0;
    return GetDescriptor(*this, 0, ConfigurationDescriptor::kType, config_index_,
                         buf_.data(), buf_.size(), true);
  }

  Error Device::OnConfigurationDescriptorReceived(const uint8_t* buf, size_t len) {
    printk("OnConfigurationDescriptorReceived: %p, %d, config_index_=%d\n",
        buf, len, config_index_);
    if (is_initialized_) {
      return Error::kSuccess;
    }

    const auto config_desc = DescriptorDynamicCast<ConfigurationDescriptor>(buf);

    const uint8_t* p = buf + config_desc->length;
    const auto num_interfaces = config_desc->num_interfaces;
    for (int if_index = 0; if_index < num_interfaces; ++if_index) {
      auto if_desc = DescriptorDynamicCast<InterfaceDescriptor>(p);
      if (if_desc == nullptr) {
        return Error::kInvalidDescriptor;
      }
      printk("Interface Descriptor: class=%d, sub=%d, protocol=%d\n",
          if_desc->interface_class,
          if_desc->interface_sub_class,
          if_desc->interface_protocol);

      p += if_desc->length;
      const auto num_endpoints = if_desc->num_endpoints;
      int num_endpoints_found = 0;

      while (num_endpoints_found < num_endpoints) {
        if (auto if_desc = DescriptorDynamicCast<InterfaceDescriptor>(p)) {
          return Error::kInvalidDescriptor;
        }
        if (auto ep_desc = DescriptorDynamicCast<EndpointDescriptor>(p)) {
          printk("Endpoint Descriptor: addr=0x%02x, attr=0x%02x\n",
              ep_desc->endpoint_address,
              ep_desc->attributes);
          ++num_endpoints_found;
        } else if (auto hid_desc = DescriptorDynamicCast<HIDDescriptor>(p)) {
          printk("HID Descriptor: release=0x%02x, num_desc=%d",
              hid_desc->hid_release,
              hid_desc->num_descriptors);
          for (int i = 0; i < hid_desc->num_descriptors; ++i) {
            printk(", desc_type=%d, len=%d",
                hid_desc->GetClassDescriptor(i)->descriptor_type,
                hid_desc->GetClassDescriptor(i)->descriptor_length);
          }
          printk("\n");
        }
        p += p[0];
      }
    }

    ++config_index_;
    if (config_index_ == num_configurations_) {
      is_initialized_ = true;
      return Error::kSuccess;
    }

    return GetDescriptor(*this, 0, ConfigurationDescriptor::kType, config_index_,
                         buf_.data(), buf_.size(), true);
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
