// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "gtest/gtest.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_sim_csrs.h"
#include "riscv/riscv_clint.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include <functional>

namespace mpact {
namespace sim {
namespace riscv {
namespace {

class RiscVSstcTest : public ::testing::Test {
 public:
  RiscVSstcTest() {
    state_ = new RiscVState("test", RiscVXlen::RV64, &memory_);
    // Instantiate an isolated CLINT instance (even if just for test inheritance/completeness)
    clint_ = new RiscVClint(1, state_->mip());
  }

  ~RiscVSstcTest() override {
    delete clint_;
    delete state_;
  }

  mpact::sim::util::FlatDemandMemory memory_;
  RiscVState* state_;
  RiscVClint* clint_;
};

TEST_F(RiscVSstcTest, TestSstcStimecmpInterrupt) {
  uint64_t callback_val = 0;
  bool callback_triggered = false;

  auto cb = [&callback_val, &callback_triggered](uint64_t val) {
    callback_val = val;
    callback_triggered = true;
  };

  STimeCmpCsr stimecmp_csr(state_, cb);

  EXPECT_EQ(stimecmp_csr.AsUint64(), 0);

  // Write a 64-bit value to trigger the callback
  uint64_t target_time = 0x1234567890ABCDEFULL;
  stimecmp_csr.Write(target_time);

  EXPECT_TRUE(callback_triggered);
  EXPECT_EQ(callback_val, target_time);
  EXPECT_EQ(stimecmp_csr.AsUint64(), target_time);

  // Reset flag
  callback_triggered = false;

  // Write a 32-bit value to trigger the callback
  uint32_t target_time32 = 0xCAFEBABE;
  stimecmp_csr.Write(target_time32);

  EXPECT_TRUE(callback_triggered);
  EXPECT_EQ(callback_val, target_time32);
  EXPECT_EQ(stimecmp_csr.AsUint64(), target_time32);
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
