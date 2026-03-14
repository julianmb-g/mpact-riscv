#include "riscv/rvvi_sim.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <thread>

namespace {

using mpact::sim::riscv::rvvi::AsyncFormattingDaemon;

TEST(RvviFormattingDaemonTest, TestGracefulShutdownWithoutDeadlock) {
  // Use a very long timeout (10 seconds) so a naive sleep_for would hang the test.
  AsyncFormattingDaemon daemon(10); 
  
  auto start_time = std::chrono::steady_clock::now();
  
  daemon.Start();
  
  // Stop should instantly signal the condition variable and join the thread.
  daemon.Stop();
  
  auto end_time = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  
  // If Stop() was blocked by sleep_for(10s), elapsed would be ~10000ms.
  // We expect it to be very fast (e.g., < 1000ms) because of cv_.notify_all().
  EXPECT_LT(elapsed, 1000);
}

}  // namespace
