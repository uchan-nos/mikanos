#include "usb/device.hpp"

#include "usb/descriptor.hpp"
#include "usb/setupdata.hpp"
#include "usb/classdriver/base.hpp"
#include "usb/classdriver/keyboard.hpp"

int printk(const char* format, ...);

namespace usb {
  Device::~Device() {
  }

  Error Device::StartInitialize() {
    is_initialized_ = false;
    initialize_phase_ = 1;
    return GetDescriptor(*this, 0, DeviceDescriptor::kType, 0,
                         buf_.data(), buf_.size(), true);
  }

  Error Device::OnControlOutCompleted(SetupData setup_data,
                                      const void* buf, size_t len) {
    if (setup_data.request_type.data == 0 &&
        setup_data.request == request::kSetConfiguration) {
      return OnSetConfigurationCompleted(setup_data.value & 0xffu);
    }
    return Error::kNotImplemented;
  }

  Error Device::OnControlInCompleted(SetupData setup_data,
                                     const void* buf, size_t len) {
    // we should check setup_data to distinguish
    // the issuer is GET_DESCRIPTOR from other.
    const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(buf);
    if (auto device_desc = DescriptorDynamicCast<DeviceDescriptor>(buf8)) {
      return OnDeviceDescriptorReceived(buf8, len);
    } else if (auto config_desc = DescriptorDynamicCast<ConfigurationDescriptor>(buf8)) {
      return OnConfigurationDescriptorReceived(buf8, len);
    } else {
      return Error::kNotImplemented;
    }
  }

  Error Device::OnDeviceDescriptorReceived(const uint8_t* buf, int len) {
    if (is_initialized_) {
      return Error::kSuccess;
    } else if (initialize_phase_ == 1) {
      return InitializePhase1(buf, len);
    }
    return Error::kNotImplemented;
  }

  Error Device::OnConfigurationDescriptorReceived(const uint8_t* buf, int len) {
    printk("OnConfigurationDescriptorReceived: %p, %d, config_index_=%d\n",
        buf, len, config_index_);
    if (is_initialized_) {
      return Error::kSuccess;
    } else if (initialize_phase_ == 2) {
      return InitializePhase2(buf, len);
    }
    return Error::kNotImplemented;
  }

  Error Device::OnSetConfigurationCompleted(uint8_t config_value) {
    printk("OnSetConfigurationCompleted: config_value=%d\n", config_value);
    if (initialize_phase_ == 3) {
      return InitializePhase3(config_value);
    }
    return Error::kNotImplemented;
  }

  Error Device::InitializePhase1(const uint8_t* buf, int len) {
    const auto device_desc = DescriptorDynamicCast<DeviceDescriptor>(buf);
    num_configurations_ = device_desc->num_configurations;
    config_index_ = 0;
    initialize_phase_ = 2;
    return GetDescriptor(*this, 0, ConfigurationDescriptor::kType, config_index_,
                         buf_.data(), buf_.size(), true);
  }

  Error Device::InitializePhase2(const uint8_t* buf, int len) {
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

      ClassDriver* class_driver = nullptr;
      if (if_desc->interface_class == 3 &&
          if_desc->interface_sub_class == 1) {  // HID boot interface
        if (if_desc->interface_protocol == 1) {  // keyboard
          class_driver = new HIDKeyboardDriver{};
        }
      }
      if (class_driver == nullptr) {
        return Error::kUnknownDevice;
      }

      p += if_desc->length;
      const auto num_endpoints = if_desc->num_endpoints;
      num_ep_configs_ = 0;

      while (num_ep_configs_ < num_endpoints) {
        if (auto if_desc = DescriptorDynamicCast<InterfaceDescriptor>(p)) {
          return Error::kInvalidDescriptor;
        }
        if (auto ep_desc = DescriptorDynamicCast<EndpointDescriptor>(p)) {
          auto& conf = ep_configs_[num_ep_configs_];
          conf.ep_num = ep_desc->endpoint_address.bits.number;
          conf.dir_in = ep_desc->endpoint_address.bits.dir_in;
          conf.ep_type = static_cast<EndpointType>(ep_desc->attributes.bits.transfer_type);
          conf.max_packet_size = ep_desc->max_packet_size;
          conf.interval = ep_desc->interval;
          printk("EndpointConf: ep_num=%d, dir_in=%d, ep_type=%d"
                 ", max_packet_size=%d, interval=%d\n",
                 conf.ep_num, conf.dir_in, conf.ep_type,
                 conf.max_packet_size, conf.interval);
          class_drivers_[conf.ep_num] = class_driver;
          ++num_ep_configs_;
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

      //class_driver->ConfigureEndpoints(epconfigs.data(), num_endpoints_found);
    }

    initialize_phase_ = 3;
    printk("issuing SetConfiguration\n");
    return SetConfiguration(*this, 0, config_desc->configuration_value, true);
  }

  Error Device::InitializePhase3(uint8_t config_value) {
    for (int i = 0; i < num_ep_configs_; ++i) {
      int ep_num = ep_configs_[i].ep_num;
      class_drivers_[ep_num]->SetEndpoint(ep_configs_[i]);
    }
    initialize_phase_ = 4;
    is_initialized_ = true;
    return Error::kSuccess;
  }

  Error GetDescriptor(Device& dev, int ep_num,
                      uint8_t desc_type, uint8_t desc_index,
                      void* buf, int len, bool debug) {
    SetupData setup_data{};
    setup_data.request_type.bits.direction = request_type::kIn;
    setup_data.request_type.bits.type = request_type::kStandard;
    setup_data.request_type.bits.recipient = request_type::kDevice;
    setup_data.request = request::kGetDescriptor;
    setup_data.value = (static_cast<uint16_t>(desc_type) << 8) | desc_index;
    setup_data.index = 0;
    setup_data.length = len;
    return dev.ControlIn(ep_num, setup_data, buf, len);
  }

  Error SetConfiguration(Device& dev, int ep_num,
                         uint8_t config_value, bool debug) {
    SetupData setup_data{};
    setup_data.request_type.bits.direction = request_type::kOut;
    setup_data.request_type.bits.type = request_type::kStandard;
    setup_data.request_type.bits.recipient = request_type::kDevice;
    setup_data.request = request::kSetConfiguration;
    setup_data.value = config_value;
    setup_data.index = 0;
    setup_data.length = 0;
    return dev.ControlOut(ep_num, setup_data, nullptr, 0);
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
