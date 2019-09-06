#pragma once

#include <cstdint>

struct CPURegisters {
  uint64_t rip, rflags;
  uint64_t rax, rbx, rcx, rdx, rdi, rsi, rbp, rsp;
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
} __attribute__((packed));
