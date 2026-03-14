#include "riscv/rvvi_sim.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <thread>
#include <vector>
#include <chrono>

namespace {

using mpact::sim::riscv::rvvi::SpscRingBuffer;
using mpact::sim::riscv::rvvi::TracePacket;

TEST(SpscRingBufferTest, BasicPushPop) {
  SpscRingBuffer<int, 10> buffer;
  int out = 0;
  EXPECT_FALSE(buffer.Pop(out));
  buffer.Push(42);
  buffer.Push(1337);
  EXPECT_TRUE(buffer.Pop(out));
  EXPECT_EQ(out, 42);
  EXPECT_TRUE(buffer.Pop(out));
  EXPECT_EQ(out, 1337);
  EXPECT_FALSE(buffer.Pop(out));
}

TEST(SpscRingBufferTest, ClearByProducer) {
  SpscRingBuffer<int, 10> buffer;
  buffer.Push(1);
  buffer.Push(2);
  buffer.Clear(); // Empties buffer safely
  int out = 0;
  EXPECT_FALSE(buffer.Pop(out));
  buffer.Push(3);
  EXPECT_TRUE(buffer.Pop(out));
  EXPECT_EQ(out, 3);
}

TEST(SpscRingBufferTest, ConcurrentPushPop) {
  SpscRingBuffer<int, 1024> buffer;
  constexpr int kNumItems = 100000;
  std::vector<int> consumed;

  std::thread producer([&]() {
    for (int i = 0; i < kNumItems; ++i) {
      buffer.Push(i);
    }
  });

  std::thread consumer([&]() {
    int expected = 0;
    while (expected < kNumItems) {
      int item = -1;
      if (buffer.Pop(item)) {
        EXPECT_EQ(item, expected);
        expected++;
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();
}

TEST(SpscRingBufferTest, AbortDeadlockPrevention) {
  SpscRingBuffer<int, 4> buffer;
  // Fill the buffer to force backpressure
  buffer.Push(1);
  buffer.Push(2);
  buffer.Push(3);
  
  std::thread producer([&]() {
    EXPECT_THROW({
      buffer.Push(4);
      buffer.Push(5); // This would hang if not aborted
    }, std::runtime_error);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  buffer.Abort(); // Unblocks producer
  producer.join();
}

}  // namespace

#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/generic/data_buffer.h"

using mpact::sim::riscv::rvvi::RvviMemoryMapper;

TEST(RvviMemoryMapperTest, RMWCycles) {
  auto memory = new mpact::sim::util::FlatDemandMemory();
  RvviMemoryMapper mapper(memory);
  
  mapper.AddMmioRange(0x1000, 0x2000);
  
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  auto db = db_factory.Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, 0xDEADBEEF);
  
  // Unaligned 4-byte write crossing 8-byte boundary
  mapper.Store(0x6, db);
  
  auto check_db = db_factory.Allocate<uint64_t>(1);
  mapper.Load(0x0, check_db, nullptr, nullptr);
  EXPECT_EQ((check_db->Get<uint64_t>(0) >> 48) & 0xFFFF, 0xBEEF);
  
  mapper.Load(0x8, check_db, nullptr, nullptr);
  EXPECT_EQ(check_db->Get<uint64_t>(0) & 0xFFFF, 0xDEAD);
  
  db->DecRef();
  check_db->DecRef();
}

class StrictMmioMemory : public mpact::sim::util::FlatDemandMemory {
 public:
  StrictMmioMemory(uint64_t start, uint64_t end) : start_(start), end_(end) {}
  void Load(uint64_t address, mpact::sim::generic::DataBuffer* db,
            mpact::sim::generic::Instruction* inst,
            mpact::sim::generic::ReferenceCount* context) override {
    EXPECT_FALSE(address >= start_ && address < end_) 
        << "MMIO region should not be read during an unaligned store! RMW bypass failed.";
    mpact::sim::util::FlatDemandMemory::Load(address, db, inst, context);
  }
 private:
  uint64_t start_;
  uint64_t end_;
};

TEST(RvviMemoryMapperTest, MmioExemption) {
  auto memory = new StrictMmioMemory(0x1000, 0x2000);
  RvviMemoryMapper mapper(memory);
  
  mapper.AddMmioRange(0x1000, 0x2000);
  
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  auto mmio_db = db_factory.Allocate<uint32_t>(1);
  mmio_db->Set<uint32_t>(0, 0xCAFEBABE);
  
  // Unaligned 4-byte write crossing 8-byte boundary INSIDE MMIO bounds
  mapper.Store(0x1006, mmio_db);
  
  auto check_mmio = db_factory.Allocate<uint32_t>(1);
  memory->mpact::sim::util::FlatDemandMemory::Load(0x1006, check_mmio, nullptr, nullptr);
  EXPECT_EQ(check_mmio->Get<uint32_t>(0), 0xCAFEBABE);
  
  mmio_db->DecRef();
  check_mmio->DecRef();
  delete memory;
}

TEST(RvviSimTest, RMWCycles_MMIO_Exemption) {
  auto memory = new StrictMmioMemory(0x10000000, 0x100000FF);
  RvviMemoryMapper mapper(memory);
  
  mapper.AddMmioRange(0x10000000, 0x100000FF);
  
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  auto mmio_db = db_factory.Allocate<uint32_t>(1);
  mmio_db->Set<uint32_t>(0, 0xCAFEBABE);
  
  // Unaligned 4-byte write crossing 8-byte boundary INSIDE MMIO bounds
  mapper.Store(0x10000006, mmio_db);
  
  auto check_mmio = db_factory.Allocate<uint32_t>(1);
  memory->mpact::sim::util::FlatDemandMemory::Load(0x10000006, check_mmio, nullptr, nullptr);
  EXPECT_EQ(check_mmio->Get<uint32_t>(0), 0xCAFEBABE);
  
  mmio_db->DecRef();
  check_mmio->DecRef();
  delete memory;
}
