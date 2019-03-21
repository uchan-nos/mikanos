#include "usb/device.hpp"

#include "usb/descriptor.hpp"
#include "usb/setupdata.hpp"
#include "usb/classdriver/base.hpp"
#include "usb/classdriver/keyboard.hpp"
#include "usb/classdriver/mouse.hpp"

#include "logger.hpp"

namespace {
  class ConfigurationDescriptorReader {
   public:
    ConfigurationDescriptorReader(const uint8_t* desc_buf, int len)
      : desc_buf_{desc_buf},
        desc_buf_len_{len},
        p_{desc_buf} {
    }

    const uint8_t* Next() {
      p_ += p_[0];
      if (p_ < desc_buf_ + desc_buf_len_) {
        return p_;
      }
      return nullptr;
    }

    template <class T>
    const T* Next() {
      while (auto n = Next()) {
        if (auto d = usb::DescriptorDynamicCast<T>(n)) {
          return d;
        }
      }
      return nullptr;
    }

   private:
    const uint8_t* const desc_buf_;
    const int desc_buf_len_;
    const uint8_t* p_;
  };

  usb::EndpointConfig MakeEPConfig(const usb::EndpointDescriptor& ep_desc) {
    usb::EndpointConfig conf;
    conf.ep_id = usb::EndpointID{
      ep_desc.endpoint_address.bits.number,
      ep_desc.endpoint_address.bits.dir_in == 1
    };
    conf.ep_type = static_cast<usb::EndpointType>(ep_desc.attributes.bits.transfer_type);
    conf.max_packet_size = ep_desc.max_packet_size;
    conf.interval = ep_desc.interval;
    return conf;
  }

  usb::ClassDriver* NewClassDriver(usb::Device* dev, const usb::InterfaceDescriptor& if_desc) {
    if (if_desc.interface_class == 3 &&
        if_desc.interface_sub_class == 1) {  // HID boot interface
      if (if_desc.interface_protocol == 1) {  // keyboard
        auto keyboard_driver = new usb::HIDKeyboardDriver{dev, if_desc.interface_number};
        if (usb::HIDKeyboardDriver::default_observer) {
          keyboard_driver->SubscribeKeyPush(usb::HIDKeyboardDriver::default_observer);
        }
        return keyboard_driver;
      } else if (if_desc.interface_protocol == 2) {  // mouse
        auto mouse_driver = new usb::HIDMouseDriver{dev, if_desc.interface_number};
        if (usb::HIDMouseDriver::default_observer) {
          mouse_driver->SubscribeMouseMove(usb::HIDMouseDriver::default_observer);
        }
        return mouse_driver;
      }
    }
    return nullptr;
  }

  void Log(LogLevel level, const usb::InterfaceDescriptor& if_desc) {
    Log(level, "Interface Descriptor: class=%d, sub=%d, protocol=%d\n",
        if_desc.interface_class,
        if_desc.interface_sub_class,
        if_desc.interface_protocol);
  }

  void Log(LogLevel level, const usb::EndpointConfig& conf) {
    Log(level, "EndpointConf: ep_id=%d, ep_type=%d"
        ", max_packet_size=%d, interval=%d\n",
        conf.ep_id.Address(), conf.ep_type,
        conf.max_packet_size, conf.interval);
  }

  void Log(LogLevel level, const usb::HIDDescriptor& hid_desc) {
    Log(level, "HID Descriptor: release=0x%02x, num_desc=%d",
        hid_desc.hid_release,
        hid_desc.num_descriptors);
    for (int i = 0; i < hid_desc.num_descriptors; ++i) {
      Log(level, ", desc_type=%d, len=%d",
          hid_desc.GetClassDescriptor(i)->descriptor_type,
          hid_desc.GetClassDescriptor(i)->descriptor_length);
    }
    Log(level, "\n");
  }
}

namespace usb {
  Device::~Device() {
  }

  Error Device::ControlIn(EndpointID ep_id, SetupData setup_data,
                          void* buf, int len, ClassDriver* issuer) {
    if (issuer) {
      event_waiters_.Put(setup_data, issuer);
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::ControlOut(EndpointID ep_id, SetupData setup_data,
                           const void* buf, int len, ClassDriver* issuer) {
    if (issuer) {
      event_waiters_.Put(setup_data, issuer);
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::InterruptIn(EndpointID ep_id, void* buf, int len) {
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::InterruptOut(EndpointID ep_id, void* buf, int len) {
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::StartInitialize() {
    is_initialized_ = false;
    initialize_phase_ = 1;
    return GetDescriptor(*this, kDefaultControlPipeID, DeviceDescriptor::kType, 0,
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

  Error Device::OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                                   const void* buf, int len) {
    Log(kDebug, "Device::OnControlCompleted: buf 0x%08x, len %d, dir %d\n",
        buf, len, setup_data.request_type.bits.direction);
    if (is_initialized_) {
      if (auto w = event_waiters_.Get(setup_data)) {
        return w.value()->OnControlCompleted(ep_id, setup_data, buf, len);
      }
      return MAKE_ERROR(Error::kNoWaiter);
    }

    const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(buf);
    if (initialize_phase_ == 1) {
      if (setup_data.request == request::kGetDescriptor &&
          DescriptorDynamicCast<DeviceDescriptor>(buf8)) {
        return InitializePhase1(buf8, len);
      }
      return MAKE_ERROR(Error::kInvalidPhase);
    } else if (initialize_phase_ == 2) {
      if (setup_data.request == request::kGetDescriptor &&
          DescriptorDynamicCast<ConfigurationDescriptor>(buf8)) {
        return InitializePhase2(buf8, len);
      }
      return MAKE_ERROR(Error::kInvalidPhase);
    } else if (initialize_phase_ == 3) {
      if (setup_data.request == request::kSetConfiguration) {
        return InitializePhase3(setup_data.value & 0xffu);
      }
      return MAKE_ERROR(Error::kInvalidPhase);
    }

    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error Device::OnInterruptCompleted(EndpointID ep_id, const void* buf, int len) {
    Log(kDebug, "Device::OnInterruptCompleted: ep addr %d\n", ep_id.Address());
    if (auto w = class_drivers_[ep_id.Number()]) {
      return w->OnInterruptCompleted(ep_id, buf, len);
    }
    return MAKE_ERROR(Error::kNoWaiter);
  }

  Error Device::InitializePhase1(const uint8_t* buf, int len) {
    const auto device_desc = DescriptorDynamicCast<DeviceDescriptor>(buf);
    num_configurations_ = device_desc->num_configurations;
    config_index_ = 0;
    initialize_phase_ = 2;
    Log(kDebug, "issuing GetDesc(Config): index=%d)\n", config_index_);
    return GetDescriptor(*this, kDefaultControlPipeID,
                         ConfigurationDescriptor::kType, config_index_,
                         buf_.data(), buf_.size(), true);
  }

  Error Device::InitializePhase2(const uint8_t* buf, int len) {
    auto conf_desc = DescriptorDynamicCast<ConfigurationDescriptor>(buf);
    if (conf_desc == nullptr) {
      return MAKE_ERROR(Error::kInvalidDescriptor);
    }
    ConfigurationDescriptorReader config_reader{buf, len};

    ClassDriver* class_driver = nullptr;
    while (auto if_desc = config_reader.Next<InterfaceDescriptor>()) {
      Log(kDebug, *if_desc);

      class_driver = NewClassDriver(this, *if_desc);
      if (class_driver == nullptr) {
        // 非対応デバイス．次の interface を調べる．
        continue;
      }

      num_ep_configs_ = 0;

      while (num_ep_configs_ < if_desc->num_endpoints) {
        auto desc = config_reader.Next();
        if (auto ep_desc = DescriptorDynamicCast<EndpointDescriptor>(desc)) {
          auto conf = MakeEPConfig(*ep_desc);
          Log(kDebug, conf);

          ep_configs_[num_ep_configs_] = conf;
          ++num_ep_configs_;
          class_drivers_[conf.ep_id.Number()] = class_driver;
        } else if (auto hid_desc = DescriptorDynamicCast<HIDDescriptor>(desc)) {
          Log(kDebug, *hid_desc);
        }
      }

      break;
    }

    if (!class_driver) {
      return MAKE_ERROR(Error::kSuccess);
    }
    initialize_phase_ = 3;
    Log(kDebug, "issuing SetConfiguration: conf_val=%d\n",
        conf_desc->configuration_value);
    return SetConfiguration(*this, kDefaultControlPipeID,
                            conf_desc->configuration_value, true);
  }

  Error Device::InitializePhase3(uint8_t config_value) {
    for (int i = 0; i < num_ep_configs_; ++i) {
      class_drivers_[ep_configs_[i].ep_id.Number()]->SetEndpoint(ep_configs_[i]);
    }
    initialize_phase_ = 4;
    is_initialized_ = true;
    return MAKE_ERROR(Error::kSuccess);
  }

  Error GetDescriptor(Device& dev, EndpointID ep_id,
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
    return dev.ControlIn(ep_id, setup_data, buf, len, nullptr);
  }

  Error SetConfiguration(Device& dev, EndpointID ep_id,
                         uint8_t config_value, bool debug) {
    SetupData setup_data{};
    setup_data.request_type.bits.direction = request_type::kOut;
    setup_data.request_type.bits.type = request_type::kStandard;
    setup_data.request_type.bits.recipient = request_type::kDevice;
    setup_data.request = request::kSetConfiguration;
    setup_data.value = config_value;
    setup_data.index = 0;
    setup_data.length = 0;
    return dev.ControlOut(ep_id, setup_data, nullptr, 0, nullptr);
  }
}
