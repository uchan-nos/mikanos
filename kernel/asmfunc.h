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
  void SetCR3(uint64_t value);
}
