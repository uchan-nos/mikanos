#pragma once

#include "usb/endpoint.hpp"

namespace usb::xhci {
  class Ring;
  union TRB;

  union SlotContext {
    uint32_t dwords[8];
    struct {
      uint32_t route_string : 20;
      uint32_t speed : 4;
      uint32_t : 1; // reserved
      uint32_t mtt : 1;
      uint32_t hub : 1;
      uint32_t context_entries : 5;

      uint32_t max_exit_latency : 16;
      uint32_t root_hub_port_num : 8;
      uint32_t num_ports : 8;

      // TT : Transaction Translator
      uint32_t tt_hub_slot_id : 8;
      uint32_t tt_port_num : 8;
      uint32_t ttt: 2;
      uint32_t : 4; // reserved
      uint32_t interrupter_target : 10;

      uint32_t usb_device_address : 8;
      uint32_t : 19;
      uint32_t slot_state : 5;
    } __attribute__((packed)) bits;
  } __attribute__((packed));

  union EndpointContext {
    uint32_t dwords[8];
    struct {
      uint32_t ep_state : 3;
      uint32_t : 5;
      uint32_t mult : 2;
      uint32_t max_primary_streams : 5;
      uint32_t linear_stream_array : 1;
      uint32_t interval : 8;
      uint32_t max_esit_payload_hi : 8;

      uint32_t : 1;
      uint32_t error_count : 2;
      uint32_t ep_type : 3;
      uint32_t : 1;
      uint32_t host_initiate_disable : 1;
      uint32_t max_burst_size : 8;
      uint32_t max_packet_size : 16;

      uint32_t dequeue_cycle_state : 1;
      uint32_t : 3;
      uint64_t tr_dequeue_pointer : 60;

      uint32_t average_trb_length : 16;
      uint32_t max_esit_payload_lo : 16;
    } __attribute__((packed)) bits;

    TRB* TransferRingBuffer() const {
      return reinterpret_cast<TRB*>(bits.tr_dequeue_pointer << 4);
    }

    void SetTransferRingBuffer(TRB* buffer) {
      bits.tr_dequeue_pointer = reinterpret_cast<uint64_t>(buffer) >> 4;
    }
  } __attribute__((packed));

  struct DeviceContextIndex {
    int value;

    explicit DeviceContextIndex(int dci) : value{dci} {}
    DeviceContextIndex(EndpointID ep_id) : value{ep_id.Address()} {}

    DeviceContextIndex(int ep_num, bool dir_in)
        : value{2 * ep_num + (ep_num == 0 ? 1 : dir_in)} {}

    DeviceContextIndex(const DeviceContextIndex& rhs) = default;
    DeviceContextIndex& operator =(const DeviceContextIndex& rhs) = default;
  };

  struct DeviceContext {
    SlotContext slot_context;
    EndpointContext ep_contexts[31];
  } __attribute__((packed));

  struct InputControlContext {
    uint32_t drop_context_flags;
    uint32_t add_context_flags;
    uint32_t reserved1[5];
    uint8_t configuration_value;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t reserved2;
  } __attribute__((packed));

  struct InputContext {
    InputControlContext input_control_context;
    SlotContext slot_context;
    EndpointContext ep_contexts[31];

    /** @brief Enable the slot context.
     *
     * @return Pointer to the slot context enabled.
     */
    SlotContext* EnableSlotContext() {
      input_control_context.add_context_flags |= 1;
      return &slot_context;
    }

    /** @brief Enable an endpoint.
     *
     * @param dci Device Context Index (1 .. 31)
     * @return Pointer to the endpoint context enabled.
     */
    EndpointContext* EnableEndpoint(DeviceContextIndex dci) {
      input_control_context.add_context_flags |= 1u << dci.value;
      return &ep_contexts[dci.value - 1];
    }
  } __attribute__((packed));
}
