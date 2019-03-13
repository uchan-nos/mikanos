#include "usb/device.hpp"

#include "usb/descriptor.hpp"
#include "usb/setupdata.hpp"
#include "usb/classdriver/base.hpp"
#include "usb/classdriver/keyboard.hpp"
#include "usb/classdriver/mouse.hpp"

#include "logger.hpp"

int printk(const char* format, ...);
usb::HIDMouseDriver* NewHIDMouseDriver(usb::Device* usb_device, int interface_index);

const char keycode_map[256] = {
  0,    0,    0,    0,  'a',  'b',  'c',  'd', // 0
  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l', // 8
  'm',  'n',  'o',  'p',  'q',  'r',  's',  't', // 16
  'u',  'v',  'w',  'x',  'y',  'z',  '1',  '2', // 24
  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0', // 32
  '\n', '\b', 0x08, '\t',  ' ',  '-',  '=',  '[', // 40
  ']', '\\',  '#',  ';', '\'',  '`',  ',',  '.', // 48
  '/',    0,    0,    0,    0,    0,    0,    0, // 56
  0,    0,    0,    0,    0,    0,    0,    0, // 64
  0,    0,    0,    0,    0,    0,    0,    0, // 72
  0,    0,    0,    0,  '/',  '*',  '-',  '+', // 80
  '\n',  '1',  '2',  '3',  '4',  '5',  '6',  '7', // 88
  '8',  '9',  '0',  '.', '\\',    0,    0,  '=', // 96
};

const char keycode_map_shifted[256] = {
  0,    0,    0,    0,  'A',  'B',  'C',  'D', // 0
  'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L', // 8
  'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T', // 16
  'U',  'V',  'W',  'X',  'Y',  'Z',  '!',  '@', // 24
  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')', // 32
  '\n', '\b', 0x08, '\t',  ' ',  '_',  '+',  '{', // 40
  '}',  '|',  '~',  ':',  '"',  '~',  '<',  '>', // 48
  '?',    0,    0,    0,    0,    0,    0,    0, // 56
  0,    0,    0,    0,    0,    0,    0,    0, // 64
  0,    0,    0,    0,    0,    0,    0,    0, // 72
  0,    0,    0,    0,  '/',  '*',  '-',  '+', // 80
  '\n',  '1',  '2',  '3',  '4',  '5',  '6',  '7', // 88
  '8',  '9',  '0',  '.', '\\',    0,    0,  '=', // 96
};

namespace usb {
  Device::~Device() {
  }

  Error Device::StartInitialize() {
    is_initialized_ = false;
    initialize_phase_ = 1;
    return GetDescriptor(*this, 0, DeviceDescriptor::kType, 0,
                         buf_.data(), buf_.size(), true);
  }

  Error Device::OnEndpointsConfigured() {
    for (auto class_driver : class_drivers_) {
      if (class_driver != nullptr) {
        if (auto err = class_driver->OnEndpointsConfigured()) {
          return err;
        }
      }
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::OnControlOutCompleted(SetupData setup_data,
                                      const void* buf, int len) {
    Log(kDebug, "Device::OnControlOutCompleted: buf 0x%08x, len %d\n", buf, len);
    if (setup_data.request_type.data == 0 &&
        setup_data.request == request::kSetConfiguration) {
      return OnSetConfigurationCompleted(setup_data.value & 0xffu);
    }
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error Device::OnControlInCompleted(SetupData setup_data,
                                     const void* buf, int len) {
    Log(kDebug, "Device::OnControlInCompleted: buf 0x%08x, len %d\n", buf, len);
    if (buf != buf_.data()) {
      // このイベントの発生源は Device クラスではなく，クラスドライバである
      for (auto class_driver : class_drivers_) {
        if (class_driver != nullptr) {
          if (auto err = class_driver->OnControlInCompleted(setup_data, buf, len)) {
            return err;
          }
        }
      }
      return MAKE_ERROR(Error::kSuccess);
    }

    // we should check setup_data to distinguish
    // the issuer is GET_DESCRIPTOR from other.
    const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(buf);
    if (auto device_desc = DescriptorDynamicCast<DeviceDescriptor>(buf8)) {
      return OnDeviceDescriptorReceived(buf8, len);
    } else if (auto config_desc = DescriptorDynamicCast<ConfigurationDescriptor>(buf8)) {
      return OnConfigurationDescriptorReceived(buf8, len);
    } else {
      return MAKE_ERROR(Error::kNotImplemented);
    }
  }

  Error Device::OnInterruptOutCompleted(const void* buf, int len) {
    for (auto class_driver : class_drivers_) {
      if (class_driver != nullptr) {
        if (auto err = class_driver->OnInterruptOutCompleted(buf, len)) {
          return err;
        }
      }
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::OnInterruptInCompleted(const void* buf, int len) {
    Log(kDebug, "Device::OnInterruptInCompleted\n");
    for (auto class_driver : class_drivers_) {
      if (class_driver != nullptr) {
        if (auto err = class_driver->OnInterruptInCompleted(buf, len)) {
          return err;
        }
      }
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::OnDeviceDescriptorReceived(const uint8_t* buf, int len) {
    if (is_initialized_) {
      return MAKE_ERROR(Error::kSuccess);
    } else if (initialize_phase_ == 1) {
      return InitializePhase1(buf, len);
    }
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error Device::OnConfigurationDescriptorReceived(const uint8_t* buf, int len) {
    Log(kDebug, "OnConfigurationDescriptorReceived: %p, %d, config_index_=%d\n",
        buf, len, config_index_);
    if (is_initialized_) {
      return MAKE_ERROR(Error::kSuccess);
    } else if (initialize_phase_ == 2) {
      return InitializePhase2(buf, len);
    }
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error Device::OnSetConfigurationCompleted(uint8_t config_value) {
    Log(kDebug, "OnSetConfigurationCompleted: config_value=%d\n", config_value);
    if (initialize_phase_ == 3) {
      return InitializePhase3(config_value);
    }
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error Device::InitializePhase1(const uint8_t* buf, int len) {
    const auto device_desc = DescriptorDynamicCast<DeviceDescriptor>(buf);
    num_configurations_ = device_desc->num_configurations;
    config_index_ = 0;
    initialize_phase_ = 2;
    Log(kDebug, "issuing GetDesc(Config): index=%d)\n", config_index_);
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
        return MAKE_ERROR(Error::kInvalidDescriptor);
      }
      Log(kDebug, "Interface Descriptor: class=%d, sub=%d, protocol=%d\n",
          if_desc->interface_class,
          if_desc->interface_sub_class,
          if_desc->interface_protocol);

      ClassDriver* class_driver = nullptr;
      if (if_desc->interface_class == 3 &&
          if_desc->interface_sub_class == 1) {  // HID boot interface
        if (if_desc->interface_protocol == 1) {  // keyboard
          auto keyboard_driver = new HIDKeyboardDriver{this, if_index};
          keyboard_driver->SubscribeKeyPush([](uint8_t keycode) {
            const char ascii = keycode_map[keycode];
            if (ascii != '\0') {
              printk("%c", ascii);
            }
          });
          class_driver = keyboard_driver;
        } else if (if_desc->interface_protocol == 2) {  // mouse
          class_driver = NewHIDMouseDriver(this, if_index);
        }
      }
      if (class_driver == nullptr) {
        // 非対応デバイス．次の interface を調べる．
        continue;
      }

      p += if_desc->length;
      const auto num_endpoints = if_desc->num_endpoints;
      num_ep_configs_ = 0;

      while (num_ep_configs_ < num_endpoints) {
        if (auto if_desc = DescriptorDynamicCast<InterfaceDescriptor>(p)) {
          return MAKE_ERROR(Error::kInvalidDescriptor);
        }
        if (auto ep_desc = DescriptorDynamicCast<EndpointDescriptor>(p)) {
          auto& conf = ep_configs_[num_ep_configs_];
          conf.ep_num = ep_desc->endpoint_address.bits.number;
          conf.dir_in = ep_desc->endpoint_address.bits.dir_in;
          conf.ep_type = static_cast<EndpointType>(ep_desc->attributes.bits.transfer_type);
          conf.max_packet_size = ep_desc->max_packet_size;
          conf.interval = ep_desc->interval;

          Log(kDebug, "EndpointConf: ep_num=%d, dir_in=%d, ep_type=%d"
              ", max_packet_size=%d, interval=%d\n",
              conf.ep_num, conf.dir_in, conf.ep_type,
              conf.max_packet_size, conf.interval);
          class_drivers_[conf.ep_num] = class_driver;
          ++num_ep_configs_;
        } else if (auto hid_desc = DescriptorDynamicCast<HIDDescriptor>(p)) {
          Log(kDebug, "HID Descriptor: release=0x%02x, num_desc=%d",
              hid_desc->hid_release,
              hid_desc->num_descriptors);
          for (int i = 0; i < hid_desc->num_descriptors; ++i) {
            Log(kDebug, ", desc_type=%d, len=%d",
                hid_desc->GetClassDescriptor(i)->descriptor_type,
                hid_desc->GetClassDescriptor(i)->descriptor_length);
          }
          Log(kDebug, "\n");
        }
        p += p[0];
      }
    }

    initialize_phase_ = 3;
    Log(kDebug, "issuing SetConfiguration: conf_val=%d\n",
        config_desc->configuration_value);
    return SetConfiguration(*this, 0, config_desc->configuration_value, true);
  }

  Error Device::InitializePhase3(uint8_t config_value) {
    for (int i = 0; i < num_ep_configs_; ++i) {
      int ep_num = ep_configs_[i].ep_num;
      class_drivers_[ep_num]->SetEndpoint(ep_configs_[i]);
    }
    initialize_phase_ = 4;
    is_initialized_ = true;
    return MAKE_ERROR(Error::kSuccess);
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
