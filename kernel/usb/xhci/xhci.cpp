#include "usb/xhci/xhci.hpp"

#include <cstring>
#include "logger.hpp"
#include "usb/setupdata.hpp"
#include "usb/device.hpp"
#include "usb/descriptor.hpp"
#include "usb/xhci/speed.hpp"

namespace {
  using namespace usb::xhci;

  Error RegisterCommandRing(Ring* ring, MemMapRegister<CRCR_Bitmap>* crcr) {
    CRCR_Bitmap value = crcr->Read();
    value.bits.ring_cycle_state = true;
    value.bits.command_stop = false;
    value.bits.command_abort = false;
    value.SetPointer(reinterpret_cast<uint64_t>(ring->Buffer()));
    crcr->Write(value);
    return MAKE_ERROR(Error::kSuccess);
  }

  enum class ConfigPhase {
    kNotConnected,
    kWaitingAddressed,
    kResettingPort,
    kEnablingSlot,
    kAddressingDevice,
    kInitializingDevice,
    kConfiguringEndpoints,
    kConfigured,
  };
  /* root hub port はリセット処理をしてからアドレスを割り当てるまでは
   * 他の処理を挟まず，そのポートについての処理だけをしなければならない．
   * kWaitingAddressed はリセット（kResettingPort）からアドレス割り当て
   * （kAddressingDevice）までの一連の処理の実行を待っている状態．
   */

  std::array<volatile ConfigPhase, 256> port_config_phase{};  // index: port number

  /** kResettingPort から kAddressingDevice までの処理を実行中のポート番号．
   * 0 ならその状態のポートがないことを示す．
   */
  uint8_t addressing_port{0};

  void InitializeSlotContext(SlotContext& ctx, Port& port) {
    ctx.bits.route_string = 0;
    ctx.bits.root_hub_port_num = port.Number();
    ctx.bits.context_entries = 1;
    ctx.bits.speed = port.Speed();
  }

  unsigned int DetermineMaxPacketSizeForControlPipe(unsigned int slot_speed) {
    switch (slot_speed) {
    case 4: // Super Speed
        return 512;
    case 3: // High Speed
        return 64;
    default:
        return 8;
    }
  }

  int MostSignificantBit(uint32_t value) {
    if (value == 0) {
      return -1;
    }

    int msb_index;
    __asm__("bsr %1, %0"
        : "=r"(msb_index) : "m"(value));
    return msb_index;
  }

  void InitializeEP0Context(EndpointContext& ctx,
                            Ring* transfer_ring,
                            unsigned int max_packet_size) {
    ctx.bits.ep_type = 4; // Control Endpoint. Bidirectional.
    ctx.bits.max_packet_size = max_packet_size;
    ctx.bits.max_burst_size = 0;
    ctx.SetTransferRingBuffer(transfer_ring->Buffer());
    ctx.bits.dequeue_cycle_state = 1;
    ctx.bits.interval = 0;
    ctx.bits.max_primary_streams = 0;
    ctx.bits.mult = 0;
    ctx.bits.error_count = 3;
  }

  Error ResetPort(Controller& xhc, Port& port) {
    const bool is_connected = port.IsConnected();
    Log(kDebug, "ResetPort: port.IsConnected() = %s\n",
        is_connected ? "true" : "false");

    if (!is_connected) {
      return MAKE_ERROR(Error::kSuccess);
    }

    if (addressing_port != 0) {
      port_config_phase[port.Number()] = ConfigPhase::kWaitingAddressed;
    } else {
      const auto port_phase = port_config_phase[port.Number()];
      if (port_phase != ConfigPhase::kNotConnected &&
          port_phase != ConfigPhase::kWaitingAddressed) {
        return MAKE_ERROR(Error::kInvalidPhase);
      }
      addressing_port = port.Number();
      port_config_phase[port.Number()] = ConfigPhase::kResettingPort;
      port.Reset();
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error EnableSlot(Controller& xhc, Port& port) {
    const bool is_enabled = port.IsEnabled();
    const bool reset_completed = port.IsPortResetChanged();
    Log(kDebug, "EnableSlot: port.IsEnabled() = %s, port.IsPortResetChanged() = %s\n",
        is_enabled ? "true" : "false",
        reset_completed ? "true" : "false");

    if (is_enabled && reset_completed) {
      port.ClearPortResetChange();

      port_config_phase[port.Number()] = ConfigPhase::kEnablingSlot;

      EnableSlotCommandTRB cmd{};
      xhc.CommandRing()->Push(cmd);
      xhc.DoorbellRegisterAt(0)->Ring(0);
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error AddressDevice(Controller& xhc, uint8_t port_id, uint8_t slot_id) {
    Log(kDebug, "AddressDevice: port_id = %d, slot_id = %d\n", port_id, slot_id);

    xhc.DeviceManager()->AllocDevice(slot_id, xhc.DoorbellRegisterAt(slot_id));

    Device* dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if (dev == nullptr) {
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    memset(&dev->InputContext()->input_control_context, 0,
           sizeof(InputControlContext));

    const auto ep0_dci = DeviceContextIndex(0, false);
    auto slot_ctx = dev->InputContext()->EnableSlotContext();
    auto ep0_ctx = dev->InputContext()->EnableEndpoint(ep0_dci);

    auto port = xhc.PortAt(port_id);
    InitializeSlotContext(*slot_ctx, port);

    InitializeEP0Context(
        *ep0_ctx, dev->AllocTransferRing(ep0_dci, 32),
        DetermineMaxPacketSizeForControlPipe(slot_ctx->bits.speed));

    xhc.DeviceManager()->LoadDCBAA(slot_id);

    port_config_phase[port_id] = ConfigPhase::kAddressingDevice;

    AddressDeviceCommandTRB addr_dev_cmd{dev->InputContext(), slot_id};
    xhc.CommandRing()->Push(addr_dev_cmd);
    xhc.DoorbellRegisterAt(0)->Ring(0);

    return MAKE_ERROR(Error::kSuccess);
  }

  Error InitializeDevice(Controller& xhc, uint8_t port_id, uint8_t slot_id) {
    Log(kDebug, "InitializeDevice: port_id = %d, slot_id = %d\n", port_id, slot_id);

    auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if (dev == nullptr) {
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    port_config_phase[port_id] = ConfigPhase::kInitializingDevice;
    dev->StartInitialize();

    return MAKE_ERROR(Error::kSuccess);
  }

  Error CompleteConfiguration(Controller& xhc, uint8_t port_id, uint8_t slot_id) {
    Log(kDebug, "CompleteConfiguration: port_id = %d, slot_id = %d\n", port_id, slot_id);

    auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if (dev == nullptr) {
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    dev->OnEndpointsConfigured();

    port_config_phase[port_id] = ConfigPhase::kConfigured;
    return MAKE_ERROR(Error::kSuccess);
  }

  Error OnEvent(Controller& xhc, PortStatusChangeEventTRB& trb) {
    Log(kDebug, "PortStatusChangeEvent: port_id = %d\n", trb.bits.port_id);
    auto port_id = trb.bits.port_id;
    auto port = xhc.PortAt(port_id);

    switch (port_config_phase[port_id]) {
    case ConfigPhase::kNotConnected:
      return ResetPort(xhc, port);
    case ConfigPhase::kResettingPort:
      return EnableSlot(xhc, port);
    default:
      return MAKE_ERROR(Error::kInvalidPhase);
    }
  }

  Error OnEvent(Controller& xhc, TransferEventTRB& trb) {
    const uint8_t slot_id = trb.bits.slot_id;
    auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if (dev == nullptr) {
      return MAKE_ERROR(Error::kInvalidSlotID);
    }
    if (auto err = dev->OnTransferEventReceived(trb)) {
      return err;
    }

    const auto port_id = dev->DeviceContext()->slot_context.bits.root_hub_port_num;
    if (dev->IsInitialized() &&
        port_config_phase[port_id] == ConfigPhase::kInitializingDevice) {
      return ConfigureEndpoints(xhc, *dev);
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error OnEvent(Controller& xhc, CommandCompletionEventTRB& trb) {
    const auto issuer_type = trb.Pointer()->bits.trb_type;
    const auto slot_id = trb.bits.slot_id;
    Log(kDebug, "CommandCompletionEvent: slot_id = %d, issuer = %s\n",
        trb.bits.slot_id, kTRBTypeToName[issuer_type]);

    if (issuer_type == EnableSlotCommandTRB::Type) {
      if (port_config_phase[addressing_port] != ConfigPhase::kEnablingSlot) {
        return MAKE_ERROR(Error::kInvalidPhase);
      }

      return AddressDevice(xhc, addressing_port, slot_id);
    } else if (issuer_type == AddressDeviceCommandTRB::Type) {
      auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
      if (dev == nullptr) {
        return MAKE_ERROR(Error::kInvalidSlotID);
      }

      auto port_id = dev->DeviceContext()->slot_context.bits.root_hub_port_num;

      if (port_id != addressing_port) {
        return MAKE_ERROR(Error::kInvalidPhase);
      }
      if (port_config_phase[port_id] != ConfigPhase::kAddressingDevice) {
        return MAKE_ERROR(Error::kInvalidPhase);
      }

      addressing_port = 0;
      for (int i = 0; i < port_config_phase.size(); ++i) {
        if (port_config_phase[i] == ConfigPhase::kWaitingAddressed) {
          auto port = xhc.PortAt(i);
          if (auto err = ResetPort(xhc, port); err) {
            return err;
          }
          break;
        }
      }

      return InitializeDevice(xhc, port_id, slot_id);
    } else if (issuer_type == ConfigureEndpointCommandTRB::Type) {
      auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
      if (dev == nullptr) {
        return MAKE_ERROR(Error::kInvalidSlotID);
      }

      auto port_id = dev->DeviceContext()->slot_context.bits.root_hub_port_num;
      if (port_config_phase[port_id] != ConfigPhase::kConfiguringEndpoints) {
        return MAKE_ERROR(Error::kInvalidPhase);
      }

      return CompleteConfiguration(xhc, port_id, slot_id);
    }

    return MAKE_ERROR(Error::kInvalidPhase);
  }

  void RequestHCOwnership(uintptr_t mmio_base, HCCPARAMS1_Bitmap hccp) {
    ExtendedRegisterList extregs{ mmio_base, hccp };

    auto ext_usblegsup = std::find_if(
        extregs.begin(), extregs.end(),
        [](auto& reg) { return reg.Read().bits.capability_id == 1; });

    if (ext_usblegsup == extregs.end()) {
      return;
    }

    auto& reg =
      reinterpret_cast<MemMapRegister<USBLEGSUP_Bitmap>&>(*ext_usblegsup);
    auto r = reg.Read();
    if (r.bits.hc_os_owned_semaphore) {
      return;
    }

    r.bits.hc_os_owned_semaphore = 1;
    Log(kDebug, "waiting until OS owns xHC...\n");
    reg.Write(r);

    do {
      r = reg.Read();
    } while (r.bits.hc_bios_owned_semaphore ||
             !r.bits.hc_os_owned_semaphore);
    Log(kDebug, "OS has owned xHC\n");
  }
}

namespace usb::xhci {

  Controller::Controller(uintptr_t mmio_base)
      : mmio_base_{mmio_base},
        cap_{reinterpret_cast<CapabilityRegisters*>(mmio_base)},
        op_{reinterpret_cast<OperationalRegisters*>(
            mmio_base + cap_->CAPLENGTH.Read())},
        max_ports_{static_cast<uint8_t>(
            cap_->HCSPARAMS1.Read().bits.max_ports)} {
  }

  Error Controller::Initialize() {
    if (auto err = devmgr_.Initialize(kDeviceSize)) {
      return err;
    }

    RequestHCOwnership(mmio_base_, cap_->HCCPARAMS1.Read());

    auto usbcmd = op_->USBCMD.Read();
    usbcmd.bits.interrupter_enable = false;
    usbcmd.bits.host_system_error_enable = false;
    usbcmd.bits.enable_wrap_event = false;
    // Host controller must be halted before resetting it.
    if (!op_->USBSTS.Read().bits.host_controller_halted) {
      usbcmd.bits.run_stop = false;  // stop
    }

    op_->USBCMD.Write(usbcmd);
    while (!op_->USBSTS.Read().bits.host_controller_halted);

    // Reset controller
    usbcmd = op_->USBCMD.Read();
    usbcmd.bits.host_controller_reset = true;
    op_->USBCMD.Write(usbcmd);
    while (op_->USBCMD.Read().bits.host_controller_reset);
    while (op_->USBSTS.Read().bits.controller_not_ready);

    Log(kDebug, "MaxSlots: %u\n", cap_->HCSPARAMS1.Read().bits.max_device_slots);
    // Set "Max Slots Enabled" field in CONFIG.
    auto config = op_->CONFIG.Read();
    config.bits.max_device_slots_enabled = kDeviceSize;
    op_->CONFIG.Write(config);

    auto hcsparams2 = cap_->HCSPARAMS2.Read();
    const uint16_t max_scratchpad_buffers =
      hcsparams2.bits.max_scratchpad_buffers_low
      | (hcsparams2.bits.max_scratchpad_buffers_high << 5);
    if (max_scratchpad_buffers > 0) {
      auto scratchpad_buf_arr = AllocArray<void*>(max_scratchpad_buffers, 64, 4096);
      for (int i = 0; i < max_scratchpad_buffers; ++i) {
        scratchpad_buf_arr[i] = AllocMem(4096, 4096, 4096);
        Log(kDebug, "scratchpad buffer array %d = %p\n",
            i, scratchpad_buf_arr[i]);
      }
      devmgr_.DeviceContexts()[0] = reinterpret_cast<DeviceContext*>(scratchpad_buf_arr);
      Log(kInfo, "wrote scratchpad buffer array %p to dev ctx array 0\n",
          scratchpad_buf_arr);
    }

    DCBAAP_Bitmap dcbaap{};
    dcbaap.SetPointer(reinterpret_cast<uint64_t>(devmgr_.DeviceContexts()));
    op_->DCBAAP.Write(dcbaap);

    auto primary_interrupter = &InterrupterRegisterSets()[0];
    if (auto err = cr_.Initialize(32)) {
        return err;
    }
    if (auto err = RegisterCommandRing(&cr_, &op_->CRCR)) {
        return err; }
    if (auto err = er_.Initialize(32, primary_interrupter)) {
        return err;
    }

    // Enable interrupt for the primary interrupter
    auto iman = primary_interrupter->IMAN.Read();
    iman.bits.interrupt_pending = true;
    iman.bits.interrupt_enable = true;
    primary_interrupter->IMAN.Write(iman);

    // Enable interrupt for the controller
    usbcmd = op_->USBCMD.Read();
    usbcmd.bits.interrupter_enable = true;
    op_->USBCMD.Write(usbcmd);

    return MAKE_ERROR(Error::kSuccess);
  }

  Error Controller::Run() {
    // Run the controller
    auto usbcmd = op_->USBCMD.Read();
    usbcmd.bits.run_stop = true;
    op_->USBCMD.Write(usbcmd);
    op_->USBCMD.Read();

    while (op_->USBSTS.Read().bits.host_controller_halted);

    return MAKE_ERROR(Error::kSuccess);
  }

  DoorbellRegister* Controller::DoorbellRegisterAt(uint8_t index) {
    return &DoorbellRegisters()[index];
  }

  Error ConfigurePort(Controller& xhc, Port& port) {
    if (port_config_phase[port.Number()] == ConfigPhase::kNotConnected) {
      return ResetPort(xhc, port);
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error ConfigureEndpoints(Controller& xhc, Device& dev) {
    const auto configs = dev.EndpointConfigs();
    const auto len = dev.NumEndpointConfigs();

    memset(&dev.InputContext()->input_control_context, 0, sizeof(InputControlContext));
    memcpy(&dev.InputContext()->slot_context,
           &dev.DeviceContext()->slot_context, sizeof(SlotContext));

    auto slot_ctx = dev.InputContext()->EnableSlotContext();
    slot_ctx->bits.context_entries = 31;
    const auto port_id{dev.DeviceContext()->slot_context.bits.root_hub_port_num};
    const int port_speed{xhc.PortAt(port_id).Speed()};
    if (port_speed == 0 || port_speed > kSuperSpeedPlus) {
      return MAKE_ERROR(Error::kUnknownXHCISpeedID);
    }

    auto convert_interval{
      (port_speed == kFullSpeed || port_speed == kLowSpeed)
      ? [](EndpointType type, int interval) { // for FS, LS
        if (type == EndpointType::kIsochronous) return interval + 2;
        else return MostSignificantBit(interval) + 3;
      }
      : [](EndpointType type, int interval) { // for HS, SS, SSP
        return interval - 1;
      }};

    for (int i = 0; i < len; ++i) {
      const DeviceContextIndex ep_dci{configs[i].ep_id};
      auto ep_ctx = dev.InputContext()->EnableEndpoint(ep_dci);
      switch (configs[i].ep_type) {
      case EndpointType::kControl:
        ep_ctx->bits.ep_type = 4;
        break;
      case EndpointType::kIsochronous:
        ep_ctx->bits.ep_type = configs[i].ep_id.IsIn() ? 5 : 1;
        break;
      case EndpointType::kBulk:
        ep_ctx->bits.ep_type = configs[i].ep_id.IsIn() ? 6 : 2;
        break;
      case EndpointType::kInterrupt:
        ep_ctx->bits.ep_type = configs[i].ep_id.IsIn() ? 7 : 3;
        break;
      }
      ep_ctx->bits.max_packet_size = configs[i].max_packet_size;
      ep_ctx->bits.interval = convert_interval(configs[i].ep_type, configs[i].interval);
      ep_ctx->bits.average_trb_length = 1;

      auto tr = dev.AllocTransferRing(ep_dci, 32);
      ep_ctx->SetTransferRingBuffer(tr->Buffer());

      ep_ctx->bits.dequeue_cycle_state = 1;
      ep_ctx->bits.max_primary_streams = 0;
      ep_ctx->bits.mult = 0;
      ep_ctx->bits.error_count = 3;
    }

    port_config_phase[port_id] = ConfigPhase::kConfiguringEndpoints;

    ConfigureEndpointCommandTRB cmd{dev.InputContext(), dev.SlotID()};
    xhc.CommandRing()->Push(cmd);
    xhc.DoorbellRegisterAt(0)->Ring(0);

    return MAKE_ERROR(Error::kSuccess);
  }

  Error ProcessEvent(Controller& xhc) {
    if (!xhc.PrimaryEventRing()->HasFront()) {
      return MAKE_ERROR(Error::kSuccess);
    }

    Error err = MAKE_ERROR(Error::kNotImplemented);
    auto event_trb = xhc.PrimaryEventRing()->Front();
    if (auto trb = TRBDynamicCast<TransferEventTRB>(event_trb)) {
      err = OnEvent(xhc, *trb);
    } else if (auto trb = TRBDynamicCast<PortStatusChangeEventTRB>(event_trb)) {
      err = OnEvent(xhc, *trb);
    } else if (auto trb = TRBDynamicCast<CommandCompletionEventTRB>(event_trb)) {
      err = OnEvent(xhc, *trb);
    }
    xhc.PrimaryEventRing()->Pop();

    return err;
  }
}
