#include "usb/xhci/xhci.hpp"

#include "usb/setupdata.hpp"
#include "usb/device.hpp"
#include "usb/descriptor.hpp"

int printk(const char* format, ...);

namespace {
  using namespace usb::xhci;

  Error RegisterCommandRing(Ring* ring, MemMapRegister<CRCR_Bitmap>* crcr) {
    CRCR_Bitmap value{};
    value.bits.ring_cycle_state = true;
    value.SetPointer(reinterpret_cast<uint64_t>(ring->Buffer()));
    crcr->Write(value);
    return MAKE_ERROR(Error::kSuccess);
  }

  enum class ConfigPhase {
    kNotConnected,
    kResettingPort,
    kEnablingSlot,
    kAddressingDevice,
    kInitializingDevice,
    kConfiguringEndpoints,
    kConfigured,
  };

  std::array<ConfigPhase, 256> port_config_phase{};  // index: port number
  std::array<uint8_t, 4> port_waiting_slot{};  // value: port number
  int num_port_waiting_slot = 0;

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

  Error ResetPort(Controller& xhc, Port& port, bool debug = true) {
    const bool is_connected = port.IsConnected();
    if (debug) {
      printk("ResetPort: port.IsConnected() = %s\n",
          is_connected ? "true" : "false");
    }

    if (is_connected) {
      port.Reset();
      port_config_phase[port.Number()] = ConfigPhase::kResettingPort;
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error EnableSlot(Controller& xhc, Port& port, bool debug = true) {
    const bool is_enabled = port.IsEnabled();
    const bool reset_completed = port.IsPortResetChanged();
    if (debug) {
      printk("EnableSlot: port.IsEnabled() = %s, port.IsPortResetChanged() = %s\n",
          is_enabled ? "true" : "false",
          reset_completed ? "true" : "false");
    }

    if (is_enabled && reset_completed) {
      port.ClearPortResetChange();

      if (num_port_waiting_slot >= port_waiting_slot.size()) {
        return MAKE_ERROR(Error::kFull);
      }
      port_waiting_slot[num_port_waiting_slot] = port.Number();
      ++num_port_waiting_slot;

      EnableSlotCommandTRB cmd{};
      xhc.CommandRing()->Push(cmd);
      xhc.DoorbellRegisterAt(0)->Ring(0);

      port_config_phase[port.Number()] = ConfigPhase::kEnablingSlot;
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error AddressDevice(Controller& xhc, uint8_t port_id, uint8_t slot_id, bool debug = true) {
    if (debug) {
      printk("AddressDevice: port_id = %d, slot_id = %d\n", port_id, slot_id);
    }

    xhc.DeviceManager()->AllocDevice(slot_id, xhc.DoorbellRegisterAt(slot_id));

    Device* dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if (dev == nullptr) {
      if (debug) printk("failed to get a device: slot = %d\n", slot_id);
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

    AddressDeviceCommandTRB addr_dev_cmd{dev->InputContext(), slot_id};
    xhc.CommandRing()->Push(addr_dev_cmd);
    xhc.DoorbellRegisterAt(0)->Ring(0);

    port_config_phase[port_id] = ConfigPhase::kAddressingDevice;

    return MAKE_ERROR(Error::kSuccess);
  }

  Error InitializeDevice(Controller& xhc, uint8_t port_id, uint8_t slot_id, bool debug = true) {
    if (debug) {
      printk("InitializeDevice: port_id = %d, slot_id = %d\n", port_id, slot_id);
    }

    auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if (dev == nullptr) {
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    dev->StartInitialize();
    port_config_phase[port_id] = ConfigPhase::kInitializingDevice;

    return MAKE_ERROR(Error::kSuccess);
  }

  Error CompleteConfiguration(Controller& xhc, uint8_t port_id, uint8_t slot_id, bool debug = true) {
    if (debug) {
      printk("CompleteConfiguration: port_id = %d, slot_id = %d\n", port_id, slot_id);
    }

    auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if (dev == nullptr) {
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    dev->OnEndpointsConfigured();

    port_config_phase[port_id] = ConfigPhase::kConfigured;
    return MAKE_ERROR(Error::kSuccess);
  }

  Error OnEvent(Controller& xhc, PortStatusChangeEventTRB& trb) {
    printk("PortStatusChangeEvent: port_id = %d\n", trb.bits.port_id);
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
    printk("CommandCompletionEvent: slot_id = %d, issuer = %s\n",
        trb.bits.slot_id,
        kTRBTypeToName[issuer_type]);

    if (issuer_type == EnableSlotCommandTRB::Type) {
      if (num_port_waiting_slot <= 0) {
        return MAKE_ERROR(Error::kEmpty);
      }
      auto port_id = port_waiting_slot[num_port_waiting_slot - 1];

      if (port_config_phase[port_id] != ConfigPhase::kEnablingSlot) {
        return MAKE_ERROR(Error::kInvalidPhase);
      }

      --num_port_waiting_slot;
      return AddressDevice(xhc, port_id, slot_id);
    } else if (issuer_type == AddressDeviceCommandTRB::Type) {
      auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
      if (dev == nullptr) {
        return MAKE_ERROR(Error::kInvalidSlotID);
      }

      auto port_id = dev->DeviceContext()->slot_context.bits.root_hub_port_num;
      if (port_config_phase[port_id] != ConfigPhase::kAddressingDevice) {
        return MAKE_ERROR(Error::kInvalidPhase);
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

    // Host controller must be halted before resetting it.
    if (!op_->USBSTS.Read().bits.host_controller_halted) {
      auto usbcmd = op_->USBCMD.Read();
      usbcmd.bits.run_stop = false;  // stop
      op_->USBCMD.Write(usbcmd);
      while (!op_->USBSTS.Read().bits.host_controller_halted);
    }

    // Reset controller
    auto usbcmd = op_->USBCMD.Read();
    usbcmd.bits.host_controller_reset = true;
    op_->USBCMD.Write(usbcmd);
    while (op_->USBCMD.Read().bits.host_controller_reset);
    while (op_->USBSTS.Read().bits.controller_not_ready);

    //printk("MaxSlots: %u\n", cap_->HCSPARAMS1.Read().bits.max_device_slots);
    // Set "Max Slots Enabled" field in CONFIG.
    auto config = op_->CONFIG.Read();
    config.bits.max_device_slots_enabled = kDeviceSize;
    op_->CONFIG.Write(config);

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

    for (int i = 0; i < len; ++i) {
      const DeviceContextIndex ep_dci{configs[i].ep_num, configs[i].dir_in};
      auto ep_ctx = dev.InputContext()->EnableEndpoint(ep_dci);
      switch (configs[i].ep_type) {
      case EndpointType::kControl:
        ep_ctx->bits.ep_type = 4;
        break;
      case EndpointType::kIsochronous:
        ep_ctx->bits.ep_type = configs[i].dir_in ? 5 : 1;
        break;
      case EndpointType::kBulk:
        ep_ctx->bits.ep_type = configs[i].dir_in ? 6 : 2;
        break;
      case EndpointType::kInterrupt:
        ep_ctx->bits.ep_type = configs[i].dir_in ? 7 : 3;
        break;
      }
      ep_ctx->bits.max_packet_size = configs[i].max_packet_size;
      ep_ctx->bits.interval = configs[i].interval;
      ep_ctx->bits.average_trb_length = 1;

      auto tr = dev.AllocTransferRing(ep_dci, 32);
      ep_ctx->SetTransferRingBuffer(tr->Buffer());

      ep_ctx->bits.dequeue_cycle_state = 1;
      ep_ctx->bits.max_primary_streams = 0;
      ep_ctx->bits.mult = 0;
      ep_ctx->bits.error_count = 3;
    }

    ConfigureEndpointCommandTRB cmd{dev.InputContext(), dev.SlotID()};
    xhc.CommandRing()->Push(cmd);
    xhc.DoorbellRegisterAt(0)->Ring(0);

    auto port_id = dev.DeviceContext()->slot_context.bits.root_hub_port_num;
    port_config_phase[port_id] = ConfigPhase::kConfiguringEndpoints;

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
