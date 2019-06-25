/**
 * @file usb/xhci/device.hpp
 *
 * USB デバイスを表すクラスと関連機能．
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "error.hpp"
#include "usb/device.hpp"
#include "usb/arraymap.hpp"
#include "usb/xhci/context.hpp"
#include "usb/xhci/trb.hpp"
#include "usb/xhci/registers.hpp"

namespace usb::xhci {
  class Device : public usb::Device {
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

    Device(uint8_t slot_id, DoorbellRegister* dbreg);

    Error Initialize();

    DeviceContext* DeviceContext() { return &ctx_; }
    InputContext* InputContext() { return &input_ctx_; }
    //usb::Device* USBDevice() { return usb_device_; }
    //void SetUSBDevice(usb::Device* value) { usb_device_ = value; }

    State State() const { return state_; }
    uint8_t SlotID() const { return slot_id_; }

    void SelectForSlotAssignment();
    Ring* AllocTransferRing(DeviceContextIndex index, size_t buf_size);

    Error ControlIn(EndpointID ep_id, SetupData setup_data,
                    void* buf, int len, ClassDriver* issuer) override;
    Error ControlOut(EndpointID ep_id, SetupData setup_data,
                     const void* buf, int len, ClassDriver* issuer) override;
    Error InterruptIn(EndpointID ep_id, void* buf, int len) override;
    Error InterruptOut(EndpointID ep_id, void* buf, int len) override;

    Error OnTransferEventReceived(const TransferEventTRB& trb);

   private:
    alignas(64) struct DeviceContext ctx_;
    alignas(64) struct InputContext input_ctx_;

    const uint8_t slot_id_;
    DoorbellRegister* const dbreg_;

    enum State state_;
    std::array<Ring*, 31> transfer_rings_; // index = dci - 1

    /** コントロール転送が完了した際に DataStageTRB や StatusStageTRB
     * から対応する SetupStageTRB を検索するためのマップ．
     */
    ArrayMap<const void*, const SetupStageTRB*, 16> setup_stage_map_{};

    //usb::Device* usb_device_;
  };
}
