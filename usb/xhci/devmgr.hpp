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
#include <memory>

namespace usb::xhci
{
  class DeviceManager
  {

  public:
    std::unique_ptr<Error> Initialize(size_t max_slots);
    DeviceContext **DeviceContexts() const;
    Device *FindByPort(uint8_t port_num, uint32_t route_string) const;
    Device *FindByState(enum Device::State state) const;
    Device *FindBySlot(uint8_t slot_id) const;
    // Withstd::unique_ptr<Error><Device*> Get(uint8_t device_id) const;
    std::unique_ptr<Error> AllocDevice(uint8_t slot_id, DoorbellRegister *dbreg);
    std::unique_ptr<Error> LoadDCBAA(uint8_t slot_id);
    std::unique_ptr<Error> Remove(uint8_t slot_id);

  private:
    // device_context_pointers_ can be used as DCBAAP's value.
    // The number of elements is max_slots_ + 1.
    DeviceContext **device_context_pointers_;
    size_t max_slots_;

    // The number of elements is max_slots_ + 1.
    Device **devices_;
  };
}
