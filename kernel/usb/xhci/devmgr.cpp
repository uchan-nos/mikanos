#include "usb/xhci/devmgr.hpp"

#include "usb/memory.hpp"

namespace usb::xhci {
  Error DeviceManager::Initialize(size_t max_slots) {
    max_slots_ = max_slots;

    devices_ = AllocArray<Device*>(max_slots_ + 1, 0, 0);
    if (devices_ == nullptr) {
      return MAKE_ERROR(Error::kNoEnoughMemory);
    }

    device_context_pointers_ = AllocArray<DeviceContext*>(max_slots_ + 1, 64, 4096);
    if (device_context_pointers_ == nullptr) {
      FreeMem(devices_);
      return MAKE_ERROR(Error::kNoEnoughMemory);
    }

    for (size_t i = 0; i <= max_slots_; ++i) {
      devices_[i] = nullptr;
      device_context_pointers_[i] = nullptr;
    }

    return MAKE_ERROR(Error::kSuccess);
  }

  DeviceContext** DeviceManager::DeviceContexts() const {
    return device_context_pointers_;
  }

  Device* DeviceManager::FindByPort(uint8_t port_num, uint32_t route_string) const {
    for (size_t i = 1; i <= max_slots_; ++i) {
      auto dev = devices_[i];
      if (dev == nullptr) continue;
      if (dev->DeviceContext()->slot_context.bits.root_hub_port_num == port_num) {
        return dev;
      }
    }
    return nullptr;
  }

  Device* DeviceManager::FindByState(enum Device::State state) const {
    for (size_t i = 1; i <= max_slots_; ++i) {
      auto dev = devices_[i];
      if (dev == nullptr) continue;
      if (dev->State() == state) {
        return dev;
      }
    }
    return nullptr;
  }

  Device* DeviceManager::FindBySlot(uint8_t slot_id) const {
    if (slot_id > max_slots_) {
      return nullptr;
    }
    return devices_[slot_id];
  }

  /*
  WithError<Device*> DeviceManager::Get(uint8_t device_id) const {
    if (device_id >= num_devices_) {
      return {nullptr, Error::kInvalidDeviceId};
    }
    return {&devices_[device_id], Error::kSuccess};
  }
  */

  Error DeviceManager::AllocDevice(uint8_t slot_id, DoorbellRegister* dbreg) {
    if (slot_id > max_slots_) {
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    if (devices_[slot_id] != nullptr) {
      return MAKE_ERROR(Error::kAlreadyAllocated);
    }

    devices_[slot_id] = AllocArray<Device>(1, 64, 4096);
    new(devices_[slot_id]) Device(slot_id, dbreg);
    return MAKE_ERROR(Error::kSuccess);
  }

  Error DeviceManager::LoadDCBAA(uint8_t slot_id) {
    if (slot_id > max_slots_) {
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    auto dev = devices_[slot_id];
    device_context_pointers_[slot_id] = dev->DeviceContext();
    return MAKE_ERROR(Error::kSuccess);
  }

  Error DeviceManager::Remove(uint8_t slot_id) {
    device_context_pointers_[slot_id] = nullptr;
    FreeMem(devices_[slot_id]);
    devices_[slot_id] = nullptr;
    return MAKE_ERROR(Error::kSuccess);
  }
}
