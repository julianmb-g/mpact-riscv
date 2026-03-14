// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "riscv/riscv_boot.h"

#include <cstdint>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "riscv/riscv32_decoder.h"
#include "riscv/riscv_register.h"
#include "riscv/riscv_register_aliases.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_top.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace {

using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVTop;
using ::mpact::sim::riscv::RV32Register;
using ::mpact::sim::riscv::RiscV32Decoder;
using ::mpact::sim::riscv::WriteBootHandoffRegisters;

class RiscVBootTest : public ::testing::Test {
 protected:
  RiscVBootTest() {
    memory_ = new mpact::sim::util::FlatDemandMemory();
    state_ = new RiscVState("test", RiscVXlen::RV32, memory_);
    decoder_ = new RiscV32Decoder(state_, memory_);
    for (int i = 0; i < 32; i++) {
      std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
      state_->AddRegister<RV32Register>(reg_name);
      state_->AddRegisterAlias<RV32Register>(
          reg_name, kXRegisterAliases[i]);
    }
    top_ = new RiscVTop("test_top", state_, decoder_);
  }

  ~RiscVBootTest() override {
    delete top_;
    delete state_;
    delete decoder_;
    delete memory_;
  }

  mpact::sim::util::FlatDemandMemory* memory_;
  RiscVState* state_;
  RiscV32Decoder* decoder_;
  RiscVTop* top_;
};

TEST_F(RiscVBootTest, SeedsHandoffRegisters) {
  uint64_t expected_hartid = 0x12345678ULL;
  uint64_t expected_dtb = 0x87654321ULL;
  
  auto status = WriteBootHandoffRegisters(top_, expected_hartid, expected_dtb);
  EXPECT_TRUE(status.ok()) << status.message();

  auto a0 = top_->ReadRegister("a0");
  EXPECT_TRUE(a0.ok());
  EXPECT_EQ(a0.value(), expected_hartid);

  auto a1 = top_->ReadRegister("a1");
  EXPECT_TRUE(a1.ok());
  EXPECT_EQ(a1.value(), expected_dtb);
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
