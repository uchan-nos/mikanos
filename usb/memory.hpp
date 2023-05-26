/**
 * @file usb/memory.hpp
 *
 * USB ドライバ用の動的メモリ管理機能
 */

#pragma once

#include <cstddef>

namespace usb {
  /** @brief 動的メモリ確保のためのメモリプールの最大容量（バイト） */
  static const size_t kMemoryPoolSize = 4096 * 32;

  /** @brief 指定されたバイト数のメモリ領域を確保して先頭ポインタを返す．
   *
   * 先頭アドレスが alignment に揃ったメモリ領域を確保する．
   * size <= boundary ならメモリ領域が boundary を跨がないことを保証する．
   * boundary は典型的にはページ境界を跨がないように 4096 を指定する．
   *
   * @param size        確保するメモリ領域のサイズ（バイト単位）
   * @param alignment   メモリ領域のアライメント制約．0 なら制約しない．
   * @param boundary    確保したメモリ領域が跨いではいけない境界．0 なら制約しない．
   * @return 確保できなかった場合は nullptr
   */
  void* AllocMem(size_t size, unsigned int alignment, unsigned int boundary);

  template <class T>
  T* AllocArray(size_t num_obj, unsigned int alignment, unsigned int boundary) {
    return reinterpret_cast<T*>(
        AllocMem(sizeof(T) * num_obj, alignment, boundary));
  }

  /** @brief 指定されたメモリ領域を解放する．本当に解放することは保証されない． */
  void FreeMem(void* p);

  /** @brief 標準コンテナ用のメモリアロケータ */
  template <class T, unsigned int Alignment = 64, unsigned int Boundary = 4096>
  class Allocator {
   public:
    using size_type = size_t;
    using pointer = T*;
    using value_type = T;

    Allocator() noexcept = default;
    Allocator(const Allocator&) noexcept = default;
    template <class U> Allocator(const Allocator<U>&) noexcept {}
    ~Allocator() noexcept = default;
    Allocator& operator=(const Allocator&) = default;

    pointer allocate(size_type n) {
      return AllocArray<T>(n, Alignment, Boundary);
    }

    void deallocate(pointer p, size_type num) {
      FreeMem(p);
    }
  };
}
