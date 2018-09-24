/**
 * @file usb/xhci/device.hpp
 *
 * USB デバイスを表すクラスと関連機能．
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "error.hpp"
#include "usb/xhci/context.hpp"
#include "usb/xhci/trb.hpp"

namespace usb::xhci {
  class Device {
   public:
    enum class State {
      kInvalid,
      kBlank,
      kSlotAssigning,
      kSlotAssigned
    };

    using OnTransferredCallbackType = void (
        Device* dev,
        DeviceContextIndex dci,
        int completion_code,
        int trb_transfer_length,
        TRB* issue_trb);

    Error Initialize();

    DeviceContext* DeviceContext() { return &ctx_; }
    InputContext* InputContext() { return &input_ctx_; }
    //usb::Device* USBDevice() { return usb_device_; }
    //void SetUSBDevice(usb::Device* value) { usb_device_ = value; }

    State State() const { return state_; }
    uint8_t SlotID() const { return slot_id_; }

    void SelectForSlotAssignment();
    void AssignSlot(uint8_t slot_id);
    Error AllocTransferRing(DeviceContextIndex index, size_t buf_size);

   private:
    alignas(64) struct DeviceContext ctx_;
    alignas(64) struct InputContext input_ctx_;

    enum State state_;
    uint8_t slot_id_;

    //usb::Device* usb_device_;
  };
}
