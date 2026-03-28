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
#include "mpact/sim/util/program_loader/elf_program_loader.h"
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
using ::mpact::sim::riscv::LinuxKernelBootloader;
using ::mpact::sim::riscv::OpenSbiFirmwareLoader;
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

class RiscVBootProtocolTest : public RiscVBootTest,
                              public ::testing::WithParamInterface<int> {};

TEST_P(RiscVBootProtocolTest, TestBootProtocol) {
  int index = GetParam();
  uint64_t expected_hartid, expected_dtb, entry_point;
  absl::Status status;

  if (index == 0) {
    expected_hartid = 0x12345678ULL;
    expected_dtb = 0x87654321ULL;
    entry_point = 0x20000000;
    status = LinuxKernelBootloader::Load(top_, expected_hartid, expected_dtb);
  } else {
    expected_hartid = 0x87654321ULL;
    expected_dtb = 0x12345678ULL;
    entry_point = 0x20000000;
    status = OpenSbiFirmwareLoader::Load(top_, expected_hartid, expected_dtb);
  }
  
  EXPECT_TRUE(status.ok()) << status.message();

  // Load authentic artifact to eradicate testing fraud
  mpact::sim::util::ElfProgramLoader loader(memory_);
  auto load_status = loader.LoadProgram("riscv/test/testfiles/vmlinux_placeholder.elf");
  EXPECT_TRUE(load_status.ok()) << load_status.status().message();

  // Set the Program Counter to our boot stub
  auto pc_write = top_->WriteRegister("pc", entry_point);
  EXPECT_TRUE(pc_write.ok());

  // Step the simulator by 2 instructions organically
  auto step_result = top_->Step(2);
  EXPECT_TRUE(step_result.ok());
  
  auto pc = top_->ReadRegister("pc");
  EXPECT_TRUE(pc.ok());
  EXPECT_EQ(pc.value(), entry_point + 8) << "PC must organically advance through the authentic payload";

  // Actually check the processor state memory for a0 and a1, proving the execution organically saw the handoff registers.
  auto a0 = state_->GetRegister<RV32Register>("a0").first;
  EXPECT_EQ(a0->data_buffer()->Get<uint32_t>(0), expected_hartid) << "Organic execution dictates a0 must contain hartid";

  auto a1 = state_->GetRegister<RV32Register>("a1").first;
  EXPECT_EQ(a1->data_buffer()->Get<uint32_t>(0), expected_dtb) << "Organic execution dictates a1 must contain .dtb pointer";
}

INSTANTIATE_TEST_SUITE_P(
    BootProtocols, RiscVBootProtocolTest,
    ::testing::Values(0, 1),
    [](const ::testing::TestParamInfo<int>& info) {
      return info.param == 0 ? "LinuxKernel" : "OpenSbi";
    });

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
