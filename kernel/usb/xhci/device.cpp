#include "usb/xhci/device.hpp"

#include "logger.hpp"
#include "usb/memory.hpp"
#include "usb/xhci/ring.hpp"

namespace {
  using namespace usb::xhci;

  SetupStageTRB MakeSetupStageTRB(usb::SetupData setup_data, int transfer_type) {
    SetupStageTRB setup{};
    setup.bits.request_type = setup_data.request_type.data;
    setup.bits.request = setup_data.request;
    setup.bits.value = setup_data.value;
    setup.bits.index = setup_data.index;
    setup.bits.length = setup_data.length;
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
    return MAKE_ERROR(Error::kSuccess);
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

  Error Device::ControlIn(int ep_num, SetupData setup_data, void* buf, int len) {
    Log(kDebug, "Device::ControlIn: ep %d, buf 0x%08x, len %d\n", ep_num, buf, len);
    if (ep_num < 0 || 15 < ep_num) {
      return MAKE_ERROR(Error::kInvalidEndpointNumber);
    }

    // control endpoint must be dir_in=true
    const DeviceContextIndex dci(ep_num, true);

    Ring* tr = transfer_rings_[dci.value - 1];

    if (tr == nullptr) {
      return MAKE_ERROR(Error::kTransferRingNotSet);
    }

    auto status = StatusStageTRB{};

    if (buf) {
      auto setup_trb_position = TRBDynamicCast<SetupStageTRB>(tr->Push(
            MakeSetupStageTRB(setup_data, SetupStageTRB::kInDataStage)));
      auto data = MakeDataStageTRB(buf, len, true);
      data.bits.interrupt_on_completion = true;
      auto data_trb_position = tr->Push(data);
      tr->Push(status);

      setup_stage_map_.Put(data_trb_position, setup_trb_position);
    } else {
      auto setup_trb_position = TRBDynamicCast<SetupStageTRB>(tr->Push(
            MakeSetupStageTRB(setup_data, SetupStageTRB::kNoDataStage)));
      status.bits.direction = true;
      status.bits.interrupt_on_completion = true;
      auto status_trb_position = tr->Push(status);

      setup_stage_map_.Put(status_trb_position, setup_trb_position);
    }

    dbreg_->Ring(dci.value);

    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::ControlOut(int ep_num, SetupData setup_data,
                           const void* buf, int len) {
    Log(kDebug, "Device::ControlOut: ep %d, buf 0x%08x, len %d\n", ep_num, buf, len);
    if (ep_num < 0 || 15 < ep_num) {
      return MAKE_ERROR(Error::kInvalidEndpointNumber);
    }

    // control endpoint must be dir_in=true
    const DeviceContextIndex dci(ep_num, true);

    Ring* tr = transfer_rings_[dci.value - 1];

    if (tr == nullptr) {
      return MAKE_ERROR(Error::kTransferRingNotSet);
    }

    auto status = StatusStageTRB{};
    status.bits.direction = true;

    if (buf) {
      auto setup_trb_position = TRBDynamicCast<SetupStageTRB>(tr->Push(
            MakeSetupStageTRB(setup_data, SetupStageTRB::kOutDataStage)));
      auto data = MakeDataStageTRB(buf, len, false);
      data.bits.interrupt_on_completion = true;
      auto data_trb_position = tr->Push(data);
      tr->Push(status);

      setup_stage_map_.Put(data_trb_position, setup_trb_position);
    } else {
      auto setup_trb_position = TRBDynamicCast<SetupStageTRB>(tr->Push(
            MakeSetupStageTRB(setup_data, SetupStageTRB::kNoDataStage)));
      status.bits.interrupt_on_completion = true;
      auto status_trb_position = tr->Push(status);

      setup_stage_map_.Put(status_trb_position, setup_trb_position);
    }

    dbreg_->Ring(dci.value);

    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::InterruptIn(int ep_num, void* buf, int len) {
    const DeviceContextIndex dci(ep_num, true);

    Ring* tr = transfer_rings_[dci.value - 1];

    if (tr == nullptr) {
      return MAKE_ERROR(Error::kTransferRingNotSet);
    }

    NormalTRB normal{};
    normal.SetPointer(buf);
    normal.bits.trb_transfer_length = len;
    normal.bits.interrupt_on_completion = true;

    tr->Push(normal);
    dbreg_->Ring(dci.value);
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::InterruptOut(int ep_num, void* buf, int len) {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error Device::OnTransferEventReceived(const TransferEventTRB& trb) {
    const auto residual_length = trb.bits.trb_transfer_length;

    TRB* issuer_trb = trb.Pointer();
    if (trb.bits.completion_code != 1 /* Success */ &&
        trb.bits.completion_code != 13 /* Short Packet */) {
      Log(kWarn, "%s has been failed: %s, residual length %d, slot %d\n",
          kTRBTypeToName[issuer_trb->bits.trb_type],
          kTRBCompletionCodeToName[trb.bits.completion_code],
          trb.bits.trb_transfer_length,
          trb.bits.slot_id);
      if (auto data_trb = TRBDynamicCast<DataStageTRB>(issuer_trb)) {
        Log(kWarn,
            "  DataStageTRB: transfer length %d, buf 0x%08x, attr 0x%02x\n",
            data_trb->bits.trb_transfer_length,
            data_trb->bits.data_buffer_pointer & 0xffffffffu,
            data_trb->data[3] & 0x7fu);
      } else if (auto setup_trb = TRBDynamicCast<SetupStageTRB>(issuer_trb)) {
        Log(kWarn,
            "  SetupStage TRB: req_type=%02x, req=%02x, val=%02x, ind=%02x, len=%02x\n",
            setup_trb->bits.request_type,
            setup_trb->bits.request,
            setup_trb->bits.value,
            setup_trb->bits.index,
            setup_trb->bits.length);
      }
      return MAKE_ERROR(Error::kTransferFailed);
    }

    if (auto normal_trb = TRBDynamicCast<NormalTRB>(issuer_trb)) {
      const auto transfer_length =
        normal_trb->bits.trb_transfer_length - residual_length;
      bool dir_in = trb.bits.endpoint_id & 1;
      if (dir_in) {
        return this->OnInterruptInCompleted(normal_trb->Pointer(),
                                            transfer_length);
      } else {
        return this->OnInterruptOutCompleted(normal_trb->Pointer(),
                                             transfer_length);
      }
    }

    auto setup_stage_trb = setup_stage_map_.Get(issuer_trb);
    if (setup_stage_trb == nullptr) {
      Log(kDebug, "No Corresponding Setup Stage for issuer %s\n",
          kTRBTypeToName[issuer_trb->bits.trb_type]);
      if (auto data_trb = TRBDynamicCast<DataStageTRB>(issuer_trb)) {
        Log(kDebug,
            "  DataStageTRB: transfer length %d, buf 0x%08x, attr 0x%02x\n",
            data_trb->bits.trb_transfer_length,
            data_trb->bits.data_buffer_pointer & 0xffffffffu,
            data_trb->data[3] & 0x7fu);
      }
      return MAKE_ERROR(Error::kNoCorrespondingSetupStage);
    }
    setup_stage_map_.Delete(issuer_trb);

    SetupData setup_data{};
    setup_data.request_type.data = setup_stage_trb->bits.request_type;
    setup_data.request = setup_stage_trb->bits.request;
    setup_data.value = setup_stage_trb->bits.value;
    setup_data.index = setup_stage_trb->bits.index;
    setup_data.length = setup_stage_trb->bits.length;

    if (auto data_stage_trb = TRBDynamicCast<DataStageTRB>(issuer_trb)) {
      const auto transfer_length =
        data_stage_trb->bits.trb_transfer_length - residual_length;
      if (setup_data.request_type.bits.direction == 0) { // direction: out
        return this->OnControlOutCompleted(
            setup_data, data_stage_trb->Pointer(), transfer_length);
      } else { // direction: in
        return this->OnControlInCompleted(
            setup_data, data_stage_trb->Pointer(), transfer_length);
      }
    } else if (auto status_stage_trb = TRBDynamicCast<StatusStageTRB>(issuer_trb)) {
      if (setup_data.request_type.bits.direction == 0) { // direction: out
        return this->OnControlOutCompleted(setup_data, nullptr, 0);
      } else {
        return this->OnControlInCompleted(setup_data, nullptr, 0);
      }
    } else {
      return MAKE_ERROR(Error::kNotImplemented);
    }
  }
}
