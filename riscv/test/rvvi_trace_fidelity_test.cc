#include "gtest/gtest.h"
#include "riscv/rvvi_sim.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/generic/data_buffer.h"
#include <chrono>
#include <thread>

namespace {
using mpact::sim::riscv::rvvi::SpscRingBuffer;

TEST(RvviTraceFidelityTest, test_trace_struct_abi_alignment_64_bytes) {
  // Test trace struct alignment using the actual struct defined in the header.
  EXPECT_EQ(sizeof(rvvi_trace_event_t), 128);
  EXPECT_EQ(alignof(rvvi_trace_event_t), 64);
}

TEST(RvviTraceFidelityTest, test_spsc_ring_buffer_backpressure_yield) {
  SpscRingBuffer<int, 4> buffer;
  // Fill the buffer to force backpressure
  buffer.Push(1);
  buffer.Push(2);
  buffer.Push(3);
  
  auto start = std::chrono::steady_clock::now();
  
  EXPECT_THROW({
    buffer.Push(4);
  }, std::runtime_error);
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  
  // Validate that backpressure correctly yields and throws timeout/deadlock.
  EXPECT_GE(duration, 4900);
}

TEST(RvviTraceFidelityTest, test_rmw_trap_on_mmio) {
  auto memory = new mpact::sim::util::FlatDemandMemory();
  mpact::sim::riscv::rvvi::RvviMemoryMapper mapper(memory);
  mapper.AddMmioRange(0x02000000, 0x02010000);
  
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  auto db = db_factory.Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, 0xDEADBEEF);
  
  // Write 4 bytes at 0x01FFFFFF, which crosses into the 0x02000000 MMIO region.
  // The first block will write to 0x01FFFFF8. The second block will write to 0x02000000.
  EXPECT_THROW({
    mapper.Store(0x01FFFFFF, db);
  }, std::runtime_error);

  db->DecRef();
  delete memory;
}
}

