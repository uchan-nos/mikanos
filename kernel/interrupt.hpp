/**
 * @file interrupt.hpp
 *
 * 割り込み用のプログラムを集めたファイル．
 */

#pragma once

#include <array>
#include <cstdint>

// #@@range_begin(desc_types)
enum class DescriptorType {
  kUpper8Bytes   = 0,
  kLDT           = 2,
  kTSSAvailable  = 9,
  kTSSBusy       = 11,
  kCallGate      = 12,
  kInterruptGate = 14,
  kTrapGate      = 15,
};
// #@@range_end(desc_types)

// #@@range_begin(descriptor_attr_struct)
union InterruptDescriptorAttribute {
  uint16_t data;
  struct {
    uint16_t interrupt_stack_table : 3;
    uint16_t : 5;
    DescriptorType type : 4;
    uint16_t : 1;
    uint16_t descriptor_privilege_level : 2;
    uint16_t present : 1;
  } __attribute__((packed)) bits;
} __attribute__((packed));
// #@@range_end(descriptor_attr_struct)

// #@@range_begin(descriptor_struct)
struct InterruptDescriptor {
  uint16_t offset_low;
  uint16_t segment_selector;
  InterruptDescriptorAttribute attr;
  uint16_t offset_middle;
  uint32_t offset_high;
  uint32_t reserved;
} __attribute__((packed));
// #@@range_end(descriptor_struct)

extern std::array<InterruptDescriptor, 256> idt;

// #@@range_begin(make_idt_attr)
constexpr InterruptDescriptorAttribute MakeIDTAttr(
    DescriptorType type,
    uint8_t descriptor_privilege_level,
    bool present = true,
    uint8_t interrupt_stack_table = 0) {
  InterruptDescriptorAttribute attr{};
  attr.bits.interrupt_stack_table = interrupt_stack_table;
  attr.bits.type = type;
  attr.bits.descriptor_privilege_level = descriptor_privilege_level;
  attr.bits.present = present;
  return attr;
}
// #@@range_end(make_idt_attr)

void SetIDTEntry(InterruptDescriptor& desc,
                 InterruptDescriptorAttribute attr,
                 uint64_t offset,
                 uint16_t segment_selector);

// #@@range_begin(vector_numbers)
class InterruptVector {
 public:
  enum Number {
    kXHCI = 0x40,
  };
};
// #@@range_end(vector_numbers)

// #@@range_begin(frame_struct)
struct InterruptFrame {
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
};
// #@@range_end(frame_struct)

void NotifyEndOfInterrupt();
