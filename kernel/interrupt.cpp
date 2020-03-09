/**
 * @file interrupt.cpp
 *
 * 割り込み用のプログラムを集めたファイル．
 */

#include "interrupt.hpp"

#include <csignal>

#include "asmfunc.h"
#include "segment.hpp"
#include "timer.hpp"
#include "task.hpp"
#include "graphics.hpp"
#include "font.hpp"

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

namespace {
  __attribute__((interrupt))
  void IntHandlerXHCI(InterruptFrame* frame) {
    task_manager->SendMessage(1, Message{Message::kInterruptXHCI});
    NotifyEndOfInterrupt();
  }

  void PrintHex(uint64_t value, int width, Vector2D<int> pos) {
    for (int i = 0; i < width; ++i) {
      int x = (value >> 4 * (width - i - 1)) & 0xfu;
      if (x >= 10) {
        x += 'a' - 10;
      } else {
        x += '0';
      }
      WriteAscii(*screen_writer, pos + Vector2D<int>{8 * i, 0}, x, {0, 0, 0});
    }
  }

  void PrintFrame(InterruptFrame* frame, const char* exp_name) {
    WriteString(*screen_writer, {500, 16*0}, exp_name, {0, 0, 0});
    WriteString(*screen_writer, {500, 16*1}, "CS:RIP", {0, 0, 0});
    PrintHex(frame->cs, 4, {500 + 8*7, 16*1});
    PrintHex(frame->rip, 16, {500 + 8*12, 16*1});
    WriteString(*screen_writer, {500, 16*2}, "RFLAGS", {0, 0, 0});
    PrintHex(frame->rflags, 16, {500 + 8*7, 16*2});
    WriteString(*screen_writer, {500, 16*3}, "SS:RSP", {0, 0, 0});
    PrintHex(frame->ss, 4, {500 + 8*7, 16*3});
    PrintHex(frame->rsp, 16, {500 + 8*12, 16*3});
  }

  void KillApp(InterruptFrame* frame) {
    const auto cpl = frame->cs & 0x3;
    if (cpl != 3) {
      return;
    }

    auto& task = task_manager->CurrentTask();
    __asm__("sti");
    ExitApp(task.OSStackPointer(), 128 + SIGSEGV);
  }

  __attribute__((interrupt))
  void IntHandlerPF(InterruptFrame* frame, uint64_t error_code) {
    uint64_t cr2 = GetCR2();
    if (auto err = HandlePageFault(error_code, cr2); !err) {
      return;
    }
    KillApp(frame);
    PrintFrame(frame, "#PF");
    WriteString(*screen_writer, {500, 16*4}, "ERR", {0, 0, 0});
    PrintHex(error_code, 16, {500 + 8*4, 16*4});
    while (true) __asm__("hlt");
  }

#define FaultHandlerWithError(fault_name) \
  __attribute__((interrupt)) \
  void IntHandler ## fault_name (InterruptFrame* frame, uint64_t error_code) { \
    KillApp(frame); \
    PrintFrame(frame, "#" #fault_name); \
    WriteString(*screen_writer, {500, 16*4}, "ERR", {0, 0, 0}); \
    PrintHex(error_code, 16, {500 + 8*4, 16*4}); \
    while (true) __asm__("hlt"); \
  }

#define FaultHandlerNoError(fault_name) \
  __attribute__((interrupt)) \
  void IntHandler ## fault_name (InterruptFrame* frame) { \
    KillApp(frame); \
    PrintFrame(frame, "#" #fault_name); \
    while (true) __asm__("hlt"); \
  }

  FaultHandlerNoError(DE)
  FaultHandlerNoError(DB)
  FaultHandlerNoError(BP)
  FaultHandlerNoError(OF)
  FaultHandlerNoError(BR)
  FaultHandlerNoError(UD)
  FaultHandlerNoError(NM)
  FaultHandlerWithError(DF)
  FaultHandlerWithError(TS)
  FaultHandlerWithError(NP)
  FaultHandlerWithError(SS)
  FaultHandlerWithError(GP)
  // FaultHandlerWithError(PF)
  FaultHandlerNoError(MF)
  FaultHandlerWithError(AC)
  FaultHandlerNoError(MC)
  FaultHandlerNoError(XM)
  FaultHandlerNoError(VE)
}

void InitializeInterrupt() {
  auto set_idt_entry = [](int irq, auto handler) {
    SetIDTEntry(idt[irq],
                MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(handler),
                kKernelCS);
  };
  set_idt_entry(InterruptVector::kXHCI, IntHandlerXHCI);
  SetIDTEntry(idt[InterruptVector::kLAPICTimer],
              MakeIDTAttr(DescriptorType::kInterruptGate, 0 /* DPL */,
                          true /* present */, kISTForTimer /* IST */),
              reinterpret_cast<uint64_t>(IntHandlerLAPICTimer),
              kKernelCS);
  set_idt_entry(0,  IntHandlerDE);
  set_idt_entry(1,  IntHandlerDB);
  set_idt_entry(3,  IntHandlerBP);
  set_idt_entry(4,  IntHandlerOF);
  set_idt_entry(5,  IntHandlerBR);
  set_idt_entry(6,  IntHandlerUD);
  set_idt_entry(7,  IntHandlerNM);
  set_idt_entry(8,  IntHandlerDF);
  set_idt_entry(10, IntHandlerTS);
  set_idt_entry(11, IntHandlerNP);
  set_idt_entry(12, IntHandlerSS);
  set_idt_entry(13, IntHandlerGP);
  set_idt_entry(14, IntHandlerPF);
  set_idt_entry(16, IntHandlerMF);
  set_idt_entry(17, IntHandlerAC);
  set_idt_entry(18, IntHandlerMC);
  set_idt_entry(19, IntHandlerXM);
  set_idt_entry(20, IntHandlerVE);
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}
