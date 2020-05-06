#include "usb/memory.hpp"

#include <cstdint>
#include <cstring>
#include "logger.hpp"
#include "memory_manager.hpp"

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
#define USE_MEMMGR

#ifdef USE_MEMMGR
  void* memory_pool;
  uintptr_t alloc_ptr;
#else
  alignas(64) uint8_t memory_pool[kMemoryPoolSize];
  uintptr_t alloc_ptr = reinterpret_cast<uintptr_t>(memory_pool);
#endif

  void* AllocMem(size_t size, unsigned int alignment, unsigned int boundary) {
#ifdef USE_MEMMGR
    if (!memory_pool) {
      auto [ frame, err ] =
        memory_manager->Allocate(kMemoryPoolSize / kBytesPerFrame);
      if (err) {
        Log(kError, "failed to allocate memory for USB: %s\n", err.Name());
        return nullptr;
      }
      memory_pool = frame.Frame();
      alloc_ptr = reinterpret_cast<uint64_t>(memory_pool);
      memset(memory_pool, 0, kMemoryPoolSize);
    }
#endif
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
