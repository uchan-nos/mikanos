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

  template <class EventTRBType>
  EventTRBType* WaitEvent(Controller& xhc) {
    while (true) {
      while (!xhc.PrimaryEventRing()->HasFront());
      if (auto trb = TRBDynamicCast<EventTRBType>(
            xhc.PrimaryEventRing()->Front())) {
        return trb;
      }
      xhc.PrimaryEventRing()->Pop();
    }
  }

  template <class EventTRBType, class IssuerTRBType>
  struct EventIssuer {
    EventTRBType* event_trb;
    IssuerTRBType* issuer_trb;
  };

  template <class EventTRBType, class IssuerTRBType>
  EventIssuer<EventTRBType, IssuerTRBType> WaitEvent(Controller& xhc) {
    while (true) {
      auto event_trb = WaitEvent<EventTRBType>(xhc);
      if (auto issuer = TRBDynamicCast<IssuerTRBType>(event_trb->Pointer())) {
        return {event_trb, issuer};
      }
      xhc.PrimaryEventRing()->Pop();
    }
  }

  void ResetPort(Port& port, bool debug = false) {
    if (debug) {
      printk("Resetting port %d\n", port.Number());
    }
    port.Reset();

    if (debug) {
      printk("Waiting port %d to be enabled\n", port.Number());
    }
    while (!port.IsEnabled());
  }

  uint8_t EnableSlot(Controller& xhc, bool debug = false) {
    EnableSlotCommandTRB cmd{};
    xhc.CommandRing()->Push(cmd);
    xhc.DoorbellRegisterAt(0)->Ring(0);

    if (debug) {
      printk("Waiting reply for the enable slot command\n");
    }
    auto event =
      WaitEvent<CommandCompletionEventTRB, EnableSlotCommandTRB>(xhc);
    uint8_t slot_id = event.event_trb->bits.slot_id;

    if (debug) {
      printk("received a response of EnableSlotCommand\n");
    }
    xhc.PrimaryEventRing()->Pop();
    xhc.DeviceManager()->AllocDevice(slot_id, xhc.DoorbellRegisterAt(slot_id));

    return slot_id;
  }

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

  Error AddressDevice(Controller& xhc, Port& port, uint8_t slot_id, bool debug = false) {
    Device* dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if (dev == nullptr) {
      if (debug) printk("failed to get a device: slot = %d\n", slot_id);
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    const auto ep0_dci = DeviceContextIndex(0, false);
    auto slot_ctx = dev->InputContext()->EnableSlotContext();
    auto ep0_ctx = dev->InputContext()->EnableEndpoint(ep0_dci);

    InitializeSlotContext(*slot_ctx, port);

    InitializeEP0Context(
        *ep0_ctx, dev->AllocTransferRing(ep0_dci, 32),
        DetermineMaxPacketSizeForControlPipe(slot_ctx->bits.speed));

    xhc.DeviceManager()->LoadDCBAA(slot_id);

    AddressDeviceCommandTRB addr_dev_cmd{dev->InputContext(), slot_id};
    xhc.CommandRing()->Push(addr_dev_cmd);
    xhc.DoorbellRegisterAt(0)->Ring(0);

    auto addr_dev_cmd_event =
      WaitEvent<CommandCompletionEventTRB, AddressDeviceCommandTRB>(xhc);

    if (auto sid = addr_dev_cmd_event.event_trb->bits.slot_id; sid != slot_id) {
      if (debug) printk("Unexpected slot id: %u\n", sid);
      return MAKE_ERROR(Error::kInvalidSlotID);
    }
    xhc.PrimaryEventRing()->Pop();

    if (debug) {
      printk("Addressed a device: slot id %u, port %u, speed %u\n",
             slot_id, port.Number(), slot_ctx->bits.speed);
    }

    return MAKE_ERROR(Error::kSuccess);
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
    if (!port.IsConnected()) {
      return MAKE_ERROR(Error::kPortNotConnected);
    }

    ResetPort(port);
    auto slot_id = EnableSlot(xhc);

    if (auto err = AddressDevice(xhc, port, slot_id, true)) {
      return err;
    }

    auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if (dev == nullptr) {
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    if (auto err = dev->StartInitialize()) {
      return err;
    }

    while (!dev->IsInitialized()) {
      auto event_trb = WaitEvent<TransferEventTRB>(xhc);
      if (auto err = dev->OnTransferEventReceived(*event_trb)) {
        return err;
      }
      xhc.PrimaryEventRing()->Pop();
    }

    printk("device on slot %d has been initialized\n", slot_id);

    ConfigureEndpoints(xhc, *dev, dev->EndpointConfigs(), dev->NumEndpointConfigs());

    auto event = WaitEvent<CommandCompletionEventTRB, ConfigureEndpointCommandTRB>(xhc);
    printk("ConfigureEndpointCommandTRB completed. completion code %d (%s)\n",
        event.event_trb->bits.completion_code,
        kTRBCompletionCodeToName[event.event_trb->bits.completion_code]);
    xhc.PrimaryEventRing()->Pop();

    if (auto err = dev->OnEndpointsConfigured()) {
      printk("failed to handle EndpointsConfigured event: %s\n", err.Name());
      return err;
    }

    return MAKE_ERROR(Error::kSuccess);
  }

  Error ConfigureEndpoints(Controller& xhc, Device& dev,
                           const EndpointConfig* configs, int len) {
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

    return MAKE_ERROR(Error::kSuccess);
  }
}
