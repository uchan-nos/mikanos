/**
 * @file acpi.hpp
 *
 * ACPI テーブル定義や操作用プログラムを集めたファイル。
 */

#pragma once

#include <cstdint>

namespace acpi {

struct RSDP {
  char signature[8];
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt_address;
  uint32_t length;
  uint64_t xsdt_address;
  uint8_t extended_checksum;
  char reserved[3];

  bool IsValid() const;
} __attribute__((packed));

void Initialize(const RSDP& rsdp);

} // namespace acpi
