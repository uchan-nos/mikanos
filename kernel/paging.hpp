/**
 * @file paging.hpp
 *
 * メモリページング用のプログラムを集めたファイル．
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "error.hpp"

/** @brief 静的に確保するページディレクトリの個数
 *
 * この定数は SetupIdentityPageMap で使用される．
 * 1 つのページディレクトリには 512 個の 2MiB ページを設定できるので，
 * kPageDirectoryCount x 1GiB の仮想アドレスがマッピングされることになる．
 */
const size_t kPageDirectoryCount = 64;

/** @brief 仮想アドレス=物理アドレスとなるようにページテーブルを設定する．
 * 最終的に CR3 レジスタが正しく設定されたページテーブルを指すようになる．
 */
void SetupIdentityPageTable();

void InitializePaging();
void ResetCR3();

union LinearAddress4Level {
  uint64_t value;

  struct {
    uint64_t offset : 12;
    uint64_t page : 9;
    uint64_t dir : 9;
    uint64_t pdp : 9;
    uint64_t pml4 : 9;
    uint64_t : 16;
  } __attribute__((packed)) parts;

  int Part(int page_map_level) const {
    switch (page_map_level) {
    case 0: return parts.offset;
    case 1: return parts.page;
    case 2: return parts.dir;
    case 3: return parts.pdp;
    case 4: return parts.pml4;
    default: return 0;
    }
  }

  void SetPart(int page_map_level, int value) {
    switch (page_map_level) {
    case 0: parts.offset = value; break;
    case 1: parts.page = value; break;
    case 2: parts.dir = value; break;
    case 3: parts.pdp = value; break;
    case 4: parts.pml4 = value; break;
    }
  }
};

union PageMapEntry {
  uint64_t data;

  struct {
    uint64_t present : 1;
    uint64_t writable : 1;
    uint64_t user : 1;
    uint64_t write_through : 1;
    uint64_t cache_disable : 1;
    uint64_t accessed : 1;
    uint64_t dirty : 1;
    uint64_t huge_page : 1;
    uint64_t global : 1;
    uint64_t : 3;

    uint64_t addr : 40;
    uint64_t : 12;
  } __attribute__((packed)) bits;

  PageMapEntry* Pointer() const {
    return reinterpret_cast<PageMapEntry*>(bits.addr << 12);
  }

  void SetPointer(PageMapEntry* p) {
    bits.addr = reinterpret_cast<uint64_t>(p) >> 12;
  }
};

WithError<PageMapEntry*> NewPageMap();
Error FreePageMap(PageMapEntry* table);
Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages,
                    bool writable = true);
Error CleanPageMaps(LinearAddress4Level addr);
Error CopyPageMaps(PageMapEntry* dest, PageMapEntry* src, int part, int start);
Error HandlePageFault(uint64_t error_code, uint64_t causal_addr);
