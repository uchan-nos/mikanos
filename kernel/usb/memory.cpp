#include "usb/memory.hpp"

#include <cstdint>

namespace {
  template <class T>
  T Ceil(T value, unsigned int alignment) {
    return (value + alignment - 1) & ~static_cast<T>(alignment - 1);
  }

  template <class T, class U>
  T MaskBits(T value, U mask) {
    return value & ~static_cast<T>(mask - 1);
  }
}

namespace usb {
  alignas(64) uint8_t memory_pool[kMemoryPoolSize];
  uintptr_t alloc_ptr = reinterpret_cast<uintptr_t>(memory_pool);

  void* AllocMem(size_t size, unsigned int alignment, unsigned int boundary) {
    if (alignment > 0) {
      alloc_ptr = Ceil(alloc_ptr, alignment);
    }
    if (boundary > 0) {
      auto next_boundary = Ceil(alloc_ptr, boundary);
      if (next_boundary < alloc_ptr + size) {
        alloc_ptr = next_boundary;
      }
    }

    if (reinterpret_cast<uintptr_t>(memory_pool) + kMemoryPoolSize
        < alloc_ptr + size) {
      return nullptr;
    }

    auto p = alloc_ptr;
    alloc_ptr += size;
    return reinterpret_cast<void*>(p);
  }

  void FreeMem(void* p) {}
}
