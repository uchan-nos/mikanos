/**
 * @file usb/xhci/devmgr.hpp
 *
 * USB デバイスの管理機能．
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "error.hpp"
#include "usb/xhci/context.hpp"
#include "usb/xhci/device.hpp"

namespace usb::xhci {
  class DeviceManager {

   public:
    Error Initialize(size_t num_devices);
    DeviceContext** DeviceContexts() const;
    Device* FindByPort(uint8_t port_num, uint32_t route_string) const;
    Device* FindByState(enum Device::State state) const;
    Device* FindBySlot(uint8_t slot_id) const;
    WithError<Device*> Get(uint8_t device_id) const;
    Error AssignSlot(Device* dev, uint8_t slot_id);
    Error Remove(Device* dev);

   private:
    // device_context_pointers_ can be used as DCBAAP's value.
    // The number of elements is num_devices_ + 1.
    DeviceContext** device_context_pointers_;
    Device* devices_;
    size_t num_devices_;
  };
}
