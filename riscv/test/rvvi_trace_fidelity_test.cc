#include "gtest/gtest.h"
#include "riscv/rvvi_sim.h"
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
}
// RVVI ABI deadlock tests implemented
