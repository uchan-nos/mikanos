/**
 * @file usb/classdriver/cdc.hpp
 *
 * CDC class drivers.
 */

#pragma once

#include <cstdint>
#include <deque>

#include "usb/classdriver/base.hpp"
#include "usb/descriptor.hpp"

namespace usb::cdc {
  enum class DescriptorSubType : uint8_t {
    kHeader = 0,
    kCM = 1,    // Call Management
    kACM = 2,   // Abstract Control Management
    kUnion = 6,
  };

  struct FunctionalDescriptor {
    static const uint8_t kType = 36; // CS_INTERFACE

    uint8_t length;                       // offset 0
    uint8_t descriptor_type;              // offset 1
    DescriptorSubType descriptor_subtype; // offset 2
  } __attribute__((packed));

  struct HeaderDescriptor : public FunctionalDescriptor {
    static const auto kSubType = DescriptorSubType::kHeader;

    uint16_t cdc;
  } __attribute__((packed));

  struct CMDescriptor : public FunctionalDescriptor {
    static const auto kSubType = DescriptorSubType::kCM;

    union {
      uint8_t data;
      struct {
        uint8_t handle_call_management : 1;
        uint8_t data_interface_usable : 1;
        uint8_t : 6;
      } __attribute__((packed)) bits;
    } capabilities;
    uint8_t data_interface;
  } __attribute__((packed));

  struct ACMDescriptor : public FunctionalDescriptor {
    static const auto kSubType = DescriptorSubType::kACM;

    union {
      uint8_t data;
      struct {
        uint8_t comm_feature : 1;
        uint8_t hw_handshake : 1;
        uint8_t send_break : 1;
        uint8_t conn_notification : 1;
        uint8_t : 4;
      } __attribute__((packed)) bits;
    } capabilities;
  } __attribute__((packed));

  struct UnionDescriptor : public FunctionalDescriptor {
    static const auto kSubType = DescriptorSubType::kUnion;

    uint8_t control_interface;
    uint8_t SubordinateInterface(size_t index) const {
      return reinterpret_cast<const uint8_t*>(this)[index + 4];
    }
  } __attribute__((packed));

  template <class T>
  const T* FuncDescDynamicCast(const uint8_t* desc_data) {
    if (desc_data[1] == T::kType &&
        desc_data[2] == static_cast<uint8_t>(T::kSubType)) {
      return reinterpret_cast<const T*>(desc_data);
    }
    return nullptr;
  }

  template <class T>
  T* FuncDescDynamicCast(uint8_t* desc_data) {
    if (FuncDescDynamicCast<const T>(desc_data)) {
      return reinterpret_cast<T*>(desc_data);
    }
    return nullptr;
  }

  class CDCDriver : public ClassDriver {
   public:
    CDCDriver(Device* dev, const InterfaceDescriptor* if_comm,
              const InterfaceDescriptor* if_data);

    Error Initialize() override;
    Error SetEndpoint(const std::vector<EndpointConfig>& configs) override;
    Error OnEndpointsConfigured() override;
    Error OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                             const void* buf, int len) override;
    Error OnNormalCompleted(EndpointID ep_id, const void* buf, int len) override;

    Error SendSerial(const void* buf, int len);
    int ReceiveSerial(void* buf, int len);

   private:
    EndpointID ep_interrupt_in_, ep_bulk_in_, ep_bulk_out_;
    std::deque<uint8_t> receive_buf_;
  };

  inline CDCDriver* driver = nullptr;
}
