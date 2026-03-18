#include <thread>
#include <atomic>
#include <chrono>

#include "gtest/gtest.h"
#include "riscv/riscv_state.h"
#include "mpact/sim/generic/instruction.h"
#include "riscv/riscv_zhintpause_instructions.h"

namespace {

using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVXlen;
using ::mpact::sim::riscv::RiscVPause;

class RiscVZawrsInstructionsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    state_ = new RiscVState("test_state", RiscVXlen::RV64, nullptr, nullptr);
    instruction_ = new mpact::sim::generic::Instruction(0, state_);
    instruction_->set_size(4);
  }

  void TearDown() override {
    instruction_->DecRef();
    delete state_;
  }

  RiscVState* state_;
  mpact::sim::generic::Instruction* instruction_;
};

TEST_F(RiscVZawrsInstructionsTest, TestZawrsWrsNtoPolling) {
  // WRS.NTO uses RiscVPause which calls std::this_thread::yield()
  // Test it organically by running a concurrent task that advances a state machine.
  // We compare the number of iterations of a tight spin loop vs a yielded loop
  // against a deterministic state-machine target (rather than a brittle timer)
  // to organically prove the yield mitigates CPU starvation.
  
  auto run_loop = [&](bool use_pause) -> int {
    std::atomic<int> bg_state{0};
    const int kTargetState = 5000000;

    std::thread t([&bg_state, kTargetState]() {
      for (int i = 1; i <= kTargetState; i++) {
        // Deterministically advance state without sleep_for
        bg_state.store(i, std::memory_order_relaxed);
      }
    });

    int loops = 0;
    while (bg_state.load(std::memory_order_relaxed) < kTargetState) {
      if (use_pause) {
        RiscVPause(this->instruction_);
      } else {
        // Just do some dummy work to prevent compiler optimizing away the loop
        __asm__ volatile("nop");
      }
      loops++;
    }
    t.join();
    return loops;
  };

  int baseline_loops = run_loop(false);
  int paused_loops = run_loop(true);

  // Organically verify that the yielded loop executed significantly fewer times
  // than the tight baseline loop, proving CPU slice give-up and syscall overhead.
  EXPECT_GT(baseline_loops, 0);
  EXPECT_GT(paused_loops, 0);
  EXPECT_LT(paused_loops, baseline_loops / 2) 
      << "Yielded loop should execute fewer iterations than tight loop "
      << "(paused: " << paused_loops << ", baseline: " << baseline_loops << ")";
}

}  // namespace
