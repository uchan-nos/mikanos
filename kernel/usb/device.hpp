/**
 * @file usb/device.hpp
 *
 * USB デバイスを表すクラスと関連機能．
 */

#pragma once

#include <array>
#include <vector>

#include "error.hpp"
#include "usb/arraymap.hpp"
#include "usb/classdriver/base.hpp"
#include "usb/descriptor.hpp"
#include "usb/endpoint.hpp"
#include "usb/setupdata.hpp"

namespace usb {
  class Device {
   public:
    virtual ~Device();
    virtual Error ControlIn(EndpointID ep_id, SetupData setup_data,
                            void* buf, int len, ClassDriver* issuer);
    virtual Error ControlOut(EndpointID ep_id, SetupData setup_data,
                             const void* buf, int len, ClassDriver* issuer);
    virtual Error NormalIn(EndpointID ep_id, void* buf, int len);
    virtual Error NormalOut(EndpointID ep_id, const void* buf, int len);

    Error StartInitialize();
    bool IsInitialized() { return is_initialized_; }
    auto& EndpointConfigs() const { return ep_configs_; }
    Error OnEndpointsConfigured();

    uint8_t* Buffer() { return buf_.data(); }
    const DeviceDescriptor& DeviceDesc() const { return device_desc_; }

   protected:
    Error OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                             const void* buf, int len);
    Error OnNormalCompleted(EndpointID ep_id, const void* buf, int len);

   private:
    std::vector<ClassDriver*> class_drivers_{};

    std::array<uint8_t, 256> buf_{};
    DeviceDescriptor device_desc_;

    // following fields are used during initialization
    uint8_t num_configurations_;
    uint8_t config_index_;

    Error OnDeviceDescriptorReceived(const uint8_t* buf, int len);
    Error OnConfigurationDescriptorReceived(const uint8_t* buf, int len);
    Error OnSetConfigurationCompleted(uint8_t config_value);

    bool is_initialized_ = false;
    int initialize_phase_ = 0;
    std::vector<EndpointConfig> ep_configs_{};
    Error InitializePhase1(const uint8_t* buf, int len);
    Error InitializePhase2(const uint8_t* buf, int len);
    Error InitializePhase3(uint8_t config_value);
    Error InitializePhase4();

    /** OnControlCompleted の中で要求の発行元を特定するためのマップ構造．
     * ControlOut または ControlIn を発行したときに発行元が登録される．
     */
    ArrayMap<SetupData, ClassDriver*, 4> event_waiters_{};
  };

  Error GetDescriptor(Device& dev, EndpointID ep_id,
                      uint8_t desc_type, uint8_t desc_index,
                      void* buf, int len, bool debug = false);
  Error SetConfiguration(Device& dev, EndpointID ep_id,
                         uint8_t config_value, bool debug = false);
}
