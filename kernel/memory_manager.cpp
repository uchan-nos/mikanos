#include "memory_manager.hpp"

#include <bitset>
#include "logger.hpp"

BitmapMemoryManager::BitmapMemoryManager()
  : alloc_map_{}, range_begin_{FrameID{0}}, range_end_{FrameID{kFrameCount}} {
}

WithError<FrameID> BitmapMemoryManager::Allocate(size_t num_frames) {
  size_t start_frame_id = range_begin_.ID();
  while (true) {
    size_t i = 0;
    for (; i < num_frames; ++i) {
      if (start_frame_id + i >= range_end_.ID()) {
        return {kNullFrame, MAKE_ERROR(Error::kNoEnoughMemory)};
      }
      if (GetBit(FrameID{start_frame_id + i})) {
        // "start_frame_id + i" にあるフレームは割り当て済み
        break;
      }
    }
    if (i == num_frames) {
      // num_frames 分の空きが見つかった
      MarkAllocated(FrameID{start_frame_id}, num_frames);
      return {
        FrameID{start_frame_id},
        MAKE_ERROR(Error::kSuccess),
      };
    }
    // 次のフレームから再検索
    start_frame_id += i + 1;
  }
}

Error BitmapMemoryManager::Free(FrameID start_frame, size_t num_frames) {
  for (size_t i = 0; i < num_frames; ++i) {
    SetBit(FrameID{start_frame.ID() + i}, false);
  }
  return MAKE_ERROR(Error::kSuccess);
}

void BitmapMemoryManager::MarkAllocated(FrameID start_frame, size_t num_frames) {
  for (size_t i = 0; i < num_frames; ++i) {
    SetBit(FrameID{start_frame.ID() + i}, true);
  }
}

void BitmapMemoryManager::SetMemoryRange(FrameID range_begin, FrameID range_end) {
  range_begin_ = range_begin;
  range_end_ = range_end;
}

MemoryStat BitmapMemoryManager::Stat() const {
  size_t sum = 0;
  for (int i = range_begin_.ID() / kBitsPerMapLine;
       i < range_end_.ID() / kBitsPerMapLine; ++i) {
    sum += std::bitset<kBitsPerMapLine>(alloc_map_[i]).count();
  }
  return { sum, range_end_.ID() - range_begin_.ID() };
}

bool BitmapMemoryManager::GetBit(FrameID frame) const {
  auto line_index = frame.ID() / kBitsPerMapLine;
  auto bit_index = frame.ID() % kBitsPerMapLine;

  return (alloc_map_[line_index] & (static_cast<MapLineType>(1) << bit_index)) != 0;
}

void BitmapMemoryManager::SetBit(FrameID frame, bool allocated) {
  auto line_index = frame.ID() / kBitsPerMapLine;
  auto bit_index = frame.ID() % kBitsPerMapLine;

  if (allocated) {
    alloc_map_[line_index] |= (static_cast<MapLineType>(1) << bit_index);
  } else {
    alloc_map_[line_index] &= ~(static_cast<MapLineType>(1) << bit_index);
  }
}

extern "C" caddr_t program_break, program_break_end;

namespace {
  char memory_manager_buf[sizeof(BitmapMemoryManager)];

  Error InitializeHeap(BitmapMemoryManager& memory_manager) {
    const int kHeapFrames = 64 * 512;
    const auto heap_start = memory_manager.Allocate(kHeapFrames);
    if (heap_start.error) {
      return heap_start.error;
    }

    program_break = reinterpret_cast<caddr_t>(heap_start.value.ID() * kBytesPerFrame);
    program_break_end = program_break + kHeapFrames * kBytesPerFrame;
    return MAKE_ERROR(Error::kSuccess);
  }
}

BitmapMemoryManager* memory_manager;

void InitializeMemoryManager(const MemoryMap& memory_map) {
  ::memory_manager = new(memory_manager_buf) BitmapMemoryManager;

  const auto memory_map_base = reinterpret_cast<uintptr_t>(memory_map.buffer);
  uintptr_t available_end = 0;
  for (uintptr_t iter = memory_map_base;
       iter < memory_map_base + memory_map.map_size;
       iter += memory_map.descriptor_size) {
    auto desc = reinterpret_cast<const MemoryDescriptor*>(iter);
    if (available_end < desc->physical_start) {
      memory_manager->MarkAllocated(
          FrameID{available_end / kBytesPerFrame},
          (desc->physical_start - available_end) / kBytesPerFrame);
    }

    const auto physical_end =
      desc->physical_start + desc->number_of_pages * kUEFIPageSize;
    if (IsAvailable(static_cast<MemoryType>(desc->type))) {
      available_end = physical_end;
    } else {
      memory_manager->MarkAllocated(
          FrameID{desc->physical_start / kBytesPerFrame},
          desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
    }
  }
  memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end / kBytesPerFrame});

  if (auto err = InitializeHeap(*memory_manager)) {
    Log(kError, "failed to allocate pages: %s at %s:%d\n",
        err.Name(), err.File(), err.Line());
    exit(1);
  }
}
