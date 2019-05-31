#include <CppUTest/CommandLineTestRunner.h>
#include "memory_manager.hpp"

TEST_GROUP(MemoryManager) {
  BitmapMemoryManager mgr;

  TEST_SETUP() {}

  TEST_TEARDOWN() {}
};

TEST(MemoryManager, Allocate) {
  const auto frame1 = mgr.Allocate(3);
  const auto frame2 = mgr.Allocate(1);

  CHECK_EQUAL(0, frame1.value.ID());
  CHECK_TRUE(3 <= frame2.value.ID());
}

TEST(MemoryManager, AllocateMultipleLine) {
  const auto frame1 = mgr.Allocate(BitmapMemoryManager::kBitsPerMapLine + 3);
  const auto frame2 = mgr.Allocate(1);

  CHECK_EQUAL(0, frame1.value.ID());
  CHECK_TRUE(BitmapMemoryManager::kBitsPerMapLine + 3 <= frame2.value.ID());
}

TEST(MemoryManager, AllocateNoEnoughMemory) {
  const auto frame1 = mgr.Allocate(BitmapMemoryManager::kFrameCount + 1);

  CHECK_EQUAL(Error::kNoEnoughMemory, frame1.error.Cause());
  CHECK_EQUAL(kNullFrame.ID(), frame1.value.ID());
}

TEST(MemoryManager, Free) {
  const auto frame1 = mgr.Allocate(3);
  mgr.Free(frame1.value, 3);
  const auto frame2 = mgr.Allocate(2);

  CHECK_EQUAL(0, frame1.value.ID());
  CHECK_EQUAL(0, frame2.value.ID());
}

TEST(MemoryManager, FreeDifferentFrames) {
  const auto frame1 = mgr.Allocate(3);
  mgr.Free(frame1.value, 1);
  const auto frame2 = mgr.Allocate(2);

  CHECK_EQUAL(0, frame1.value.ID());
  CHECK_TRUE(3 <= frame2.value.ID());
}

TEST(MemoryManager, MarkAllocated) {
  mgr.MarkAllocated(FrameID{61}, 3);
  const auto frame1 = mgr.Allocate(64);

  CHECK_EQUAL(64, frame1.value.ID());
}

TEST(MemoryManager, SetMemoryRange) {
  const auto frame1 = mgr.Allocate(1);
  mgr.SetMemoryRange(FrameID{10}, FrameID{64});
  const auto frame2 = mgr.Allocate(1);

  CHECK_EQUAL(0, frame1.value.ID());
  CHECK_EQUAL(10, frame2.value.ID());
}
