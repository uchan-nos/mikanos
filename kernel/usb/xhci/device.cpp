#include "usb/xhci/device.hpp"

#include "usb/memory.hpp"
#include "usb/xhci/ring.hpp"

namespace {
  using namespace usb::xhci;

  SetupStageTRB MakeSetupStageTRB(uint64_t setup_data, int transfer_type) {
    SetupStageTRB setup{};
    setup.bits.request_type = setup_data & 0xffu;
    setup.bits.request = (setup_data >> 8) & 0xffu;
    setup.bits.value = (setup_data >> 16) & 0xffffu;
    setup.bits.index = (setup_data >> 32) & 0xffffu;
    setup.bits.length = (setup_data >> 48) & 0xffffu;
    setup.bits.transfer_type = transfer_type;
    return setup;
  }

  DataStageTRB MakeDataStageTRB(const void* buf, int len, bool dir_in) {
    DataStageTRB data{};
    data.SetPointer(buf);
    data.bits.trb_transfer_length = len;
    data.bits.td_size = 0;
    data.bits.direction = dir_in;
    return data;
  }
}

namespace usb::xhci {
  Device::Device(uint8_t slot_id, DoorbellRegister* dbreg)
      : slot_id_{slot_id}, dbreg_{dbreg} {
  }

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

  Ring* Device::AllocTransferRing(DeviceContextIndex index, size_t buf_size) {
    int i = index.value - 1;
    auto tr = AllocArray<Ring>(1, 64, 4096);
    if (tr) {
      tr->Initialize(buf_size);
    }
    transfer_rings_[i] = tr;
    return tr;
  }

  Error Device::ControlIn(int ep_num, uint64_t setup_data, void* buf, int len) {
    if (ep_num < 0 || 15 < ep_num) {
      return Error::kInvalidEndpointNumber;
    }

    // control endpoint must be dir_in=true
    const DeviceContextIndex dci(ep_num, true);

    Ring* tr = transfer_rings_[dci.value - 1];

    if (tr == nullptr) {
      return Error::kTransferRingNotSet;
    }

    auto status = StatusStageTRB{};

    if (buf) {
      tr->Push(MakeSetupStageTRB(setup_data, SetupStageTRB::kInDataStage));
      auto data = MakeDataStageTRB(buf, len, true);
      data.bits.interrupt_on_completion = true;
      tr->Push(data);
    } else {
      tr->Push(MakeSetupStageTRB(setup_data, SetupStageTRB::kNoDataStage));
      status.bits.direction = true;
    }

    tr->Push(status);

    dbreg_->Ring(dci.value);

    return Error::kSuccess;
  }

  Error Device::ControlOut(int ep_num, uint64_t setup_data,
                           const void* buf, int len) {
    return Error::kSuccess;
  }

  Error Device::OnTransferEventReceived(const TransferEventTRB& trb) {
    const auto residual_length = trb.bits.trb_transfer_length;

    TRB* issuer_trb = trb.Pointer();
    if (auto data_stage_trb = TRBDynamicCast<DataStageTRB>(issuer_trb)) {
      const auto transfer_length =
        data_stage_trb->bits.trb_transfer_length - residual_length;
      if (data_stage_trb->bits.direction == 0) { // direction: out
        this->OnControlOutCompleted(data_stage_trb->Pointer(), transfer_length);
      } else { // direction: in
        this->OnControlInCompleted(data_stage_trb->Pointer(), transfer_length);
      }
    } else {
      return Error::kNotImplemented;
    }
    return Error::kSuccess;
  }
}
