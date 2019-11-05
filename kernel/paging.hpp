/**
 * @file paging.hpp
 *
 * メモリページング用のプログラムを集めたファイル．
 */

#pragma once

#include <cstddef>
#include <cstdint>

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

union LinearAddress4Level {
  uint64_t value;

  struct {
    uint64_t offset : 12;
    uint64_t page : 9;
    uint64_t dir : 9;
    uint64_t pdp : 9;
    uint64_t pml4 : 9;
    uint64_t : 16;
  } parts __attribute__((packed));
};
