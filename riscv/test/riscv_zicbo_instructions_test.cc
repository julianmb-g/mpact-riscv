// Copyright 2024 Google LLC
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

#include "riscv/riscv_zicbo_instructions.h"

#include <cstdint>

#include "googlemock/include/gmock/gmock.h"
#include "googletest/include/gtest/gtest.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "riscv/riscv_csr.h"
#include "riscv/riscv_state.h"

namespace {

using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::ImmediateOperand;
using ::mpact::sim::util::FlatDemandMemory;

class RiscVZicboInstructionsTest : public testing::Test {
 protected:
  RiscVZicboInstructionsTest() {
    memory_ = new FlatDemandMemory();
    state_ = new RiscVState("test", mpact::sim::riscv::RiscVXlen::RV64, memory_);
    instruction_ = new Instruction(0, state_);
    instruction_->set_size(4);
  }

  ~RiscVZicboInstructionsTest() override {
    instruction_->DecRef();
    delete state_;
    delete memory_;
  }

  FlatDemandMemory* memory_;
  RiscVState* state_;
  Instruction* instruction_;
};

TEST_F(RiscVZicboInstructionsTest, CboZero) {
  // cbo.zero requires rs1. Inject unaligned address to test masking.
  instruction_->AppendSource(new ImmediateOperand<uint64_t>(0x1024));

  // Make sure memory is initially dirty.
  auto* db = state_->db_factory()->Allocate<uint8_t>(64);
  auto data = db->Get<uint8_t>();
  for (int i = 0; i < 64; ++i) {
    data[i] = 0xFF;
  }
  // Store to the aligned base address 0x1000.
  state_->StoreMemory(instruction_, 0x1000, db);
  db->DecRef();

  instruction_->set_semantic_function(&mpact::sim::riscv::RiscVCboZero);
  instruction_->Execute(nullptr);

  // Now verify it's zeroed.
  auto* read_db = state_->db_factory()->Allocate<uint8_t>(64);
  state_->LoadMemory(instruction_, 0x1000, read_db, nullptr, nullptr);
  auto read_data = read_db->Get<uint8_t>();
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(read_data[i], 0);
  }
  read_db->DecRef();
}

TEST_F(RiscVZicboInstructionsTest, CboInval) {
  instruction_->AppendSource(new ImmediateOperand<uint64_t>(0x1000));
  instruction_->set_semantic_function(&mpact::sim::riscv::RiscVCboInval);
  instruction_->Execute(nullptr);
  // No-op, just make sure it doesn't crash.
  SUCCEED();
}

TEST_F(RiscVZicboInstructionsTest, CboClean) {
  instruction_->AppendSource(new ImmediateOperand<uint64_t>(0x1000));
  instruction_->set_semantic_function(&mpact::sim::riscv::RiscVCboClean);
  instruction_->Execute(nullptr);
  SUCCEED();
}

TEST_F(RiscVZicboInstructionsTest, CboFlush) {
  instruction_->AppendSource(new ImmediateOperand<uint64_t>(0x1000));
  instruction_->set_semantic_function(&mpact::sim::riscv::RiscVCboFlush);
  instruction_->Execute(nullptr);
  SUCCEED();
}

TEST_F(RiscVZicboInstructionsTest, TestZicbozPrivilegeEscalation) {
  // cbo.zero requires rs1.
  instruction_->AppendSource(new ImmediateOperand<uint64_t>(0x1000));
  instruction_->set_semantic_function(&mpact::sim::riscv::RiscVCboZero);

  bool trap_taken = false;
  state_->set_on_trap(
      [&trap_taken](bool is_interrupt, uint64_t trap_value,
                    uint64_t exception_code, uint64_t epc,
                    const mpact::sim::riscv::Instruction* inst) -> bool {
        if (exception_code == static_cast<uint64_t>(
                mpact::sim::riscv::ExceptionCode::kIllegalInstruction)) {
          trap_taken = true;
          return true;
        }
        return false;
      });

  auto res_m = state_->csr_set()->GetCsr(
      static_cast<uint64_t>(mpact::sim::riscv::RiscVCsrEnum::kMenvcfg));
  ASSERT_TRUE(res_m.ok());
  auto* menvcfg = res_m.value();

  auto res_s = state_->csr_set()->GetCsr(
      static_cast<uint64_t>(mpact::sim::riscv::RiscVCsrEnum::kSenvcfg));
  ASSERT_TRUE(res_s.ok());
  auto* senvcfg = res_s.value();

  // Test 1: Machine mode, CBZE = 0 (Should NOT trap, Machine mode ignores CBZE)
  menvcfg->Write(static_cast<uint64_t>(0));
  senvcfg->Write(static_cast<uint64_t>(0));
  state_->set_privilege_mode(mpact::sim::riscv::PrivilegeMode::kMachine);
  trap_taken = false;
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken);

  // Test 2: Supervisor mode, menvcfg.CBZE = 0 (Should TRAP)
  menvcfg->Write(static_cast<uint64_t>(0));
  state_->set_privilege_mode(mpact::sim::riscv::PrivilegeMode::kSupervisor);
  trap_taken = false;
  instruction_->Execute(nullptr);
  EXPECT_TRUE(trap_taken);

  // Test 3: Supervisor mode, menvcfg.CBZE = 1 (Should NOT trap)
  menvcfg->Write(static_cast<uint64_t>(1ULL << 4));
  state_->set_privilege_mode(mpact::sim::riscv::PrivilegeMode::kSupervisor);
  trap_taken = false;
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken);

  // Test 4: User mode, menvcfg.CBZE = 1, senvcfg.CBZE = 0 (Should TRAP)
  menvcfg->Write(static_cast<uint64_t>(1ULL << 4));
  senvcfg->Write(static_cast<uint64_t>(0));
  state_->set_privilege_mode(mpact::sim::riscv::PrivilegeMode::kUser);
  trap_taken = false;
  instruction_->Execute(nullptr);
  EXPECT_TRUE(trap_taken);

  // Test 5: User mode, menvcfg.CBZE = 1, senvcfg.CBZE = 1 (Should NOT trap)
  menvcfg->Write(static_cast<uint64_t>(1ULL << 4));
  senvcfg->Write(static_cast<uint64_t>(1ULL << 4));
  state_->set_privilege_mode(mpact::sim::riscv::PrivilegeMode::kUser);
  trap_taken = false;
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken);
}

}  // namespace
