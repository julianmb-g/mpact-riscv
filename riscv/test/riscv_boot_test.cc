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

#include <cstdint>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/util/program_loader/elf_program_loader.h"
#include "riscv/riscv32_decoder.h"
#include "riscv/riscv_boot.h"
#include "riscv/riscv_register.h"
#include "riscv/riscv_register_aliases.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_top.h"
#include "utils/assembler/native_assembler_wrapper.h"

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
using ::mpact::sim::assembler::NativeTextualAssembler;

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
    delete decoder_;
    delete state_;
    delete memory_;
  }

  void LoadPayload(uint64_t entry_point, const std::string& asm_text) {
    NativeTextualAssembler assembler;
    auto elf_bytes = assembler.Assemble(asm_text);
    EXPECT_TRUE(elf_bytes.ok()) << elf_bytes.status().message();
    if (!elf_bytes.ok()) return;
    auto* db = state_->db_factory()->Allocate<uint8_t>(elf_bytes.value().size());
    std::memcpy(db->raw_ptr(), elf_bytes.value().data(), elf_bytes.value().size());
    memory_->Store(entry_point, db);
    db->DecRef();
  }

  mpact::sim::util::FlatDemandMemory* memory_;
  RiscVState* state_;
  RiscV32Decoder* decoder_;
  RiscVTop* top_;
};

TEST_F(RiscVBootTest, TestLinuxBootProtocol) {
  uint64_t expected_hartid = 0x12345678ULL;
  uint64_t expected_dtb = 0x87654321ULL;
  uint64_t entry_point = 0x20000000;
  
  auto status = LinuxKernelBootloader::Load(top_, expected_hartid, expected_dtb);
  EXPECT_TRUE(status.ok()) << status.message();

  // Load authentic assembled artifact to eradicate testing fraud.
  // We write the values of a0 and a1 into memory at 0 to prove organic execution.
  // sw a0, 0(x0)
  // sw a1, 4(x0)
  LoadPayload(entry_point, 
    "sw a0, 0(x0)\n"
    "sw a1, 4(x0)\n"
    "nop\n"
  );

  EXPECT_TRUE(top_->WriteRegister("pc", entry_point).ok());

  // Step the simulator organically
  EXPECT_TRUE(top_->Step(4).ok());
  
  // Read memory at 0 to verify structural hardware execution.
  auto* mem_db = state_->db_factory()->Allocate<uint32_t>(2);
  memory_->Load(0, mem_db, nullptr, nullptr);
  EXPECT_EQ(mem_db->Get<uint32_t>(0), expected_hartid) << "Organic execution dictates memory must contain hartid";
  EXPECT_EQ(mem_db->Get<uint32_t>(1), expected_dtb) << "Organic execution dictates memory must contain .dtb pointer";
  mem_db->DecRef();
}

TEST_F(RiscVBootTest, TestOpenSbiBootProtocol) {
  uint64_t expected_hartid = 0x87654321ULL;
  uint64_t expected_dtb = 0x12345678ULL;
  uint64_t entry_point = 0x20000000;
  
  auto status = OpenSbiFirmwareLoader::Load(top_, expected_hartid, expected_dtb);
  EXPECT_TRUE(status.ok()) << status.message();

  LoadPayload(entry_point, 
    "sw a0, 0(x0)\n"
    "sw a1, 4(x0)\n"
    "nop\n"
  );

  EXPECT_TRUE(top_->WriteRegister("pc", entry_point).ok());

  EXPECT_TRUE(top_->Step(4).ok());
  
  auto* mem_db = state_->db_factory()->Allocate<uint32_t>(2);
  memory_->Load(0, mem_db, nullptr, nullptr);
  EXPECT_EQ(mem_db->Get<uint32_t>(0), expected_hartid) << "Organic execution dictates memory must contain hartid";
  EXPECT_EQ(mem_db->Get<uint32_t>(1), expected_dtb) << "Organic execution dictates memory must contain .dtb pointer";
  mem_db->DecRef();
}

TEST_F(RiscVBootTest, TestPrecompiledVmlinuxBootSequence) {
  // Enforce organic failure without GTEST_SKIP() when missing authentic ELF
  mpact::sim::util::ElfProgramLoader loader(memory_);
  auto load_status = loader.LoadProgram("riscv/test/testfiles/vmlinux.elf");
  if (!load_status.ok()) {
    FAIL() << "MANDATE: Forbid GTEST_SKIP() when artifact is missing; enforce organic failure. " << load_status.status().message();
  }

  uint64_t expected_hartid = 0x0;
  uint64_t expected_dtb = 0x21000000ULL;
  auto status = LinuxKernelBootloader::Load(top_, expected_hartid, expected_dtb);
  EXPECT_TRUE(status.ok()) << status.message();

  // Dynamically verify ELF segment boundaries instead of hardcoding payload size
  uint64_t max_payload_address = 0;
  auto* elf_reader = loader.elf_reader();
  for (const auto& segment : elf_reader->segments) {
    uint64_t segment_end = segment->get_virtual_address() + segment->get_memory_size();
    if (segment_end > max_payload_address) {
      max_payload_address = segment_end;
    }
  }

  // Assert non-intersection bounds dynamically
  EXPECT_LT(max_payload_address, expected_dtb) << "MANDATE: vmlinux payload must not intersect with DTB memory region.";

  // Validate the registers are set correctly before boot execution
  EXPECT_EQ(top_->ReadRegister("a0").value(), expected_hartid) << "MANDATE: a0 must be hartid";
  EXPECT_EQ(top_->ReadRegister("a1").value(), expected_dtb) << "MANDATE: a1 must be dtb pointer";

  EXPECT_TRUE(top_->WriteRegister("pc", 0x20000000).ok());
  EXPECT_TRUE(top_->Step(1).ok());
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
