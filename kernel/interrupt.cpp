/**
 * @file interrupt.cpp
 *
 * 割り込み用のプログラムを集めたファイル．
 */

#include "interrupt.hpp"

#include "asmfunc.h"
#include "segment.hpp"

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

void NotifyEndOfInterrupt() {
  volatile auto end_of_interrupt = reinterpret_cast<uint32_t*>(0xfee000b0);
  *end_of_interrupt = 0;
}

// #@@range_begin(int_handler)
namespace {
  std::deque<Message>* msg_queue;

  __attribute__((interrupt))
  void IntHandlerXHCI(InterruptFrame* frame) {
    msg_queue->push_back(Message{Message::kInterruptXHCI});
    NotifyEndOfInterrupt();
  }

  __attribute__((interrupt))
  void IntHandlerLAPICTimer(InterruptFrame* frame) {
    msg_queue->push_back(Message{Message::kInterruptLAPICTimer});
    NotifyEndOfInterrupt();
  }
}
// #@@range_end(int_handler)

// #@@range_begin(register_handler)
void InitializeInterrupt(std::deque<Message>* msg_queue) {
  ::msg_queue = msg_queue;

  SetIDTEntry(idt[InterruptVector::kXHCI],
              MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerXHCI),
              kKernelCS);
  SetIDTEntry(idt[InterruptVector::kLAPICTimer],
              MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerLAPICTimer),
              kKernelCS);
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}
// #@@range_end(register_handler)
