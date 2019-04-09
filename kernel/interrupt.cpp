/**
 * @file interrupt.cpp
 *
 * 割り込み用のプログラムを集めたファイル．
 */

#include "interrupt.hpp"

std::array<InterruptDescriptor, 256> idt;

void SetIDTEntry(InterruptDescriptor& desc,
                 InterruptDescriptorAttribute attr,
                 uint64_t offset,
                 uint16_t segment_selector) {
  desc.attr = attr;
  desc.offset_low = offset & 0xffffu;
  desc.offset_middle = (offset >> 16) & 0xffffu;
  desc.offset_high = offset >> 32;
  desc.segment_selector = segment_selector;
}

void LoadIDT() {
  uint8_t idtr_buf[sizeof(uint16_t) + sizeof(uintptr_t)];
  const uint16_t length = sizeof(idt) - 1;
  const uintptr_t offset = reinterpret_cast<uintptr_t>(&idt[0]);

  memcpy(&idtr_buf[0], &length, sizeof(uint16_t));
  memcpy(&idtr_buf[sizeof(uint16_t)], &offset, sizeof(uintptr_t));

  __asm__("lidt %0" : : "m"(idtr_buf));
}
