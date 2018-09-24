#include "usb/xhci/devmgr.hpp"

#include "usb/memory.hpp"

namespace usb::xhci {
  Error DeviceManager::Initialize(size_t num_devices) {
    num_devices_ = num_devices;

    devices_ = AllocArray<Device>(num_devices_, 0, 0);
    if (devices_ == nullptr) {
      return Error::kNoEnoughMemory;
    }

    device_context_pointers_ = AllocArray<DeviceContext*>(num_devices_ + 1, 0, 0);
    if (device_context_pointers_ == nullptr) {
      FreeMem(devices_);
      return Error::kNoEnoughMemory;
    }

    for (size_t i = 0; i < num_devices_; ++i) {
      devices_[i].Initialize();
      device_context_pointers_[i + 1] = nullptr;
    }
    device_context_pointers_[0] = nullptr; // scratch pad buffer

    return Error::kSuccess;
  }

  DeviceContext** DeviceManager::DeviceContexts() const {
    return device_context_pointers_;
  }

  Device* DeviceManager::FindByPort(uint8_t port_num, uint32_t route_string) const {
    for (size_t i = 0; i < num_devices_; ++i) {
      auto dev = &devices_[i];
      if (dev->DeviceContext()->slot_context.bits.root_hub_port_num == port_num) {
        return dev;
      }
    }
    return nullptr;
  }

  Device* DeviceManager::FindByState(enum Device::State state) const {
    for (size_t i = 0; i < num_devices_; ++i) {
      auto dev = &devices_[i];
      if (dev->State() == state) {
        return dev;
      }
    }
    return nullptr;
  }

  Device* DeviceManager::FindBySlot(uint8_t slot_id) const {
    for (size_t i = 0; i < num_devices_; ++i) {
      auto dev = &devices_[i];
      if (dev->SlotID() == slot_id) {
        return dev;
      }
    }
    return nullptr;
  }

  WithError<Device*> DeviceManager::Get(uint8_t device_id) const {
    if (device_id >= num_devices_) {
      return {nullptr, Error::kInvalidDeviceId};
    }
    return {&devices_[device_id], Error::kSuccess};
  }

  Error DeviceManager::AssignSlot(Device* dev, uint8_t slot_id) {
    dev->AssignSlot(slot_id);
    device_context_pointers_[slot_id] = dev->DeviceContext();

    return Error::kSuccess;
  }

  Error DeviceManager::Remove(Device* dev) {
    device_context_pointers_[dev->SlotID()] = nullptr;
    dev->Initialize();
    return Error::kSuccess;
  }
}
