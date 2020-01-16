#pragma once

#include <stdint.h>

extern "C" {
  void IoOut32(uint16_t addr, uint32_t data);
  uint32_t IoIn32(uint16_t addr);
  uint16_t GetCS(void);
  void LoadIDT(uint16_t limit, uint64_t offset);
  void LoadGDT(uint16_t limit, uint64_t offset);
  void SetCSSS(uint16_t cs, uint16_t ss);
  void SetDSAll(uint16_t value);
  uint64_t GetCR3();
  void SetCR3(uint64_t value);
  void SwitchContext(uint64_t* to_rsp, uint64_t* current_rsp);
  int CallApp(int argc, char** argv, uint16_t ss, uint64_t rip, uint64_t rsp, uint64_t* os_stack_ptr);
  void WriteMSR(uint32_t msr, uint64_t value);
  void SyscallEntry(void);
}
