#include "usb/xhci/device.hpp"

#include "usb/memory.hpp"
#include "usb/xhci/ring.hpp"

namespace usb::xhci {
  Error Device::Initialize() {
    state_ = State::kBlank;
    for (size_t i = 0; i < 31; ++i) {
      const DeviceContextIndex dci(i + 1);
      //on_transferred_callbacks_[i] = nullptr;
    }
    return Error::kSuccess;
  }

  void Device::SelectForSlotAssignment() {
    state_ = State::kSlotAssigning;
  }

  void Device::AssignSlot(uint8_t slot_id) {
    slot_id_ = slot_id;
    state_ = State::kSlotAssigned;
  }

  Error Device::AllocTransferRing(DeviceContextIndex index, size_t buf_size) {
    int i = index.value - 1;
    auto tr = AllocArray<Ring>(1, 64, 4096);
    tr->Initialize(buf_size);
    //reinterpret_cast<usb::xhci::EndpointSet*>(usb_device_->EndpointSet())
    //  ->SetTransferRing(index, tr);
    return Error::kSuccess;
  }
}
