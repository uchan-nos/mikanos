/**
 * @file usb/device.hpp
 *
 * USB デバイスを表すクラスと関連機能．
 */

#pragma once

#include <array>

#include "error.hpp"
#include "usb/setupdata.hpp"
#include "usb/endpoint.hpp"

namespace usb {
  class ClassDriver;

  class Device {
   public:
    virtual ~Device();
    virtual Error ControlIn(int ep_num, SetupData setup_data,
                            void* buf, int len) = 0;
    virtual Error ControlOut(int ep_num, SetupData setup_data,
                             const void* buf, int len) = 0;
    virtual Error InterruptIn(int ep_num, void* buf, int len) = 0;
    virtual Error InterruptOut(int ep_num, void* buf, int len) = 0;

    Error StartInitialize();
    bool IsInitialized() { return is_initialized_; }
    EndpointConfig* EndpointConfigs() { return ep_configs_.data(); }
    int NumEndpointConfigs() { return num_ep_configs_; }
    Error OnEndpointsConfigured();

    uint8_t* Buffer() { return buf_.data(); }

   protected:
    Error OnControlOutCompleted(SetupData setup_data,
                                const void* buf, int len);
    Error OnControlInCompleted(SetupData setup_data,
                               const void* buf, int len);
    Error OnInterruptOutCompleted(const void* buf, int len);
    Error OnInterruptInCompleted(const void* buf, int len);

   private:
    /** @brief エンドポイントに割り当て済みのクラスドライバ．
     *
     * 添字はエンドポイント番号（0 - 15）．
     * 添字 0 はどのクラスドライバからも使われないため，常に未使用．
     */
    std::array<ClassDriver*, 16> class_drivers_{};

    std::array<uint8_t, 256> buf_{};
    bool is_initialized_ = false;

    // following fields are used during initialization
    uint8_t num_configurations_;
    uint8_t config_index_;

    Error OnDeviceDescriptorReceived(const uint8_t* buf, int len);
    Error OnConfigurationDescriptorReceived(const uint8_t* buf, int len);
    Error OnSetConfigurationCompleted(uint8_t config_value);

    int initialize_phase_ = 0;
    std::array<EndpointConfig, 16> ep_configs_;
    int num_ep_configs_;
    Error InitializePhase1(const uint8_t* buf, int len);
    Error InitializePhase2(const uint8_t* buf, int len);
    Error InitializePhase3(uint8_t config_value);
    Error InitializePhase4();
  };

  Error GetDescriptor(Device& dev, int ep_num,
                      uint8_t desc_type, uint8_t desc_index,
                      void* buf, int len, bool debug = false);
  Error SetConfiguration(Device& dev, int ep_num,
                         uint8_t config_value, bool debug = false);
}
