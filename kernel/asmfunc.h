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
  uint64_t GetCR0();
  void SetCR0(uint64_t value);
  uint64_t GetCR2();
  void SetCR3(uint64_t value);
  uint64_t GetCR3();
  void SwitchContext(void* next_ctx, void* current_ctx);
  void RestoreContext(void* ctx);
  int CallApp(int argc, char** argv, uint16_t ss, uint64_t rip, uint64_t rsp, uint64_t* os_stack_ptr);
  void IntHandlerLAPICTimer();
  void LoadTR(uint16_t sel);
  void WriteMSR(uint32_t msr, uint64_t value);
  void SyscallEntry(void);
  void ExitApp(uint64_t rsp, int32_t ret_val);
  void InvalidateTLB(uint64_t addr);
}
