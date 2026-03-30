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
#include "absl/strings/str_cat.h"
#include "riscv/riscv_csr.h"
#include "riscv/riscv_sim_csrs.h"
#include "riscv/riscv_top.h"
#include "riscv/rva23s64_decoder_wrapper.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_fp_state.h"
#include "riscv/riscv_vector_state.h"
#include "riscv/riscv_register.h"
#include "riscv/riscv_register_aliases.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/util/memory/atomic_memory.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace {

using ::mpact::sim::riscv::RiscVTop;
using ::mpact::sim::riscv::Rva23s64DecoderWrapper;
using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVFPState;
using ::mpact::sim::riscv::RiscVVectorState;
using ::mpact::sim::riscv::RiscVXlen;
using ::mpact::sim::riscv::RV64Register;
using ::mpact::sim::riscv::PrivilegeMode;
using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::util::AtomicMemory;

class RiscVSmcntrpmfTest : public ::testing::Test {
 public:
  RiscVSmcntrpmfTest() {
    memory_ = new FlatDemandMemory();
    atomic_memory_ = new AtomicMemory(memory_);
    state_ = new RiscVState("RVA23S64", RiscVXlen::RV64, memory_, atomic_memory_);
    state_->AddExtension("Smcntrpmf");

    fp_state_ = new RiscVFPState(state_->csr_set(), state_);
    state_->set_rv_fp(fp_state_);

    vector_state_ = new RiscVVectorState(state_, 64);
    state_->set_rv_vector(vector_state_);

    decoder_ = new Rva23s64DecoderWrapper(state_, memory_);

    std::string reg_name;
    for (int i = 0; i < 32; i++) {
      reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
      (void)state_->AddRegister<RV64Register>(reg_name);
      (void)state_->AddRegisterAlias<RV64Register>(
          reg_name, ::mpact::sim::riscv::kXRegisterAliases[i]);
    }

    top_ = new RiscVTop("test_top", state_, decoder_);
  }

  ~RiscVSmcntrpmfTest() override {
    delete top_;
    delete decoder_;
    delete vector_state_;
    delete fp_state_;
    delete state_;
    delete atomic_memory_;
    delete memory_;
  }

  FlatDemandMemory* memory_;
  AtomicMemory* atomic_memory_;
  RiscVState* state_;
  RiscVFPState* fp_state_;
  RiscVVectorState* vector_state_;
  Rva23s64DecoderWrapper* decoder_;
  RiscVTop* top_;
};

TEST_F(RiscVSmcntrpmfTest, TestMCycleInhibitionBoundary) {
  uint64_t inst_addr = 0x1000;
  
  // Create 10 NOPs (addi x0, x0, 0 => 0x00000013)
  uint32_t inst_nop = 0x00000013;
  for (int i = 0; i < 10; ++i) {
    auto* db = state_->db_factory()->Allocate<uint32_t>(1);
    db->Set<uint32_t>(0, inst_nop);
    memory_->Store(inst_addr + i * 4, db);
    db->DecRef();
  }

  // Setup: Set Privilege to Machine Mode
  state_->set_privilege_mode(PrivilegeMode::kMachine);
  
  // Verify mcyclecfg existence
  auto mcyclecfg_res = state_->csr_set()->GetCsr("mcyclecfg");
  ASSERT_TRUE(mcyclecfg_res.ok());
  auto* mcyclecfg = mcyclecfg_res.value();
  
  auto mcycle_res = state_->csr_set()->GetCsr("mcycle");
  ASSERT_TRUE(mcycle_res.ok());
  auto* mcycle = mcycle_res.value();

  auto minstret_res = state_->csr_set()->GetCsr("minstret");
  ASSERT_TRUE(minstret_res.ok());
  auto* minstret = minstret_res.value();

  // Read initial cycle
  uint64_t start_cycle = mcycle->AsUint64();
  uint64_t start_instret = minstret->AsUint64();

  // Step 1: Execute a NOP normally
  EXPECT_TRUE(top_->WriteRegister("pc", inst_addr).ok());
  EXPECT_TRUE(top_->Step(1).ok());

  EXPECT_EQ(mcycle->AsUint64(), start_cycle + 1) << "mcycle should increment when not inhibited";
  EXPECT_EQ(minstret->AsUint64(), start_instret + 1) << "minstret should increment when not inhibited";

  // Step 2: Inhibit counting in M-mode
  uint64_t MINH_MASK = 1ULL << 62;
  mcyclecfg->Write(MINH_MASK); // Inhibit mcycle in M-mode

  auto minstretcfg_res = state_->csr_set()->GetCsr("minstretcfg");
  ASSERT_TRUE(minstretcfg_res.ok());
  minstretcfg_res.value()->Write(MINH_MASK); // Inhibit minstret in M-mode

  // Execute another NOP
  EXPECT_TRUE(top_->Step(1).ok());

  // Values should mathematically remain identical (halted)
  EXPECT_EQ(mcycle->AsUint64(), start_cycle + 1) << "mcycle must organically halt when inhibited in M-Mode";
  EXPECT_EQ(minstret->AsUint64(), start_instret + 1) << "minstret must organically halt when inhibited in M-Mode";

  // Step 3: Shift privilege to S-mode. Since M-mode is inhibited but S-mode is NOT, it should resume natively.
  state_->set_privilege_mode(PrivilegeMode::kSupervisor);
  
  EXPECT_TRUE(top_->Step(1).ok());
  
  EXPECT_EQ(mcycle->AsUint64(), start_cycle + 2) << "mcycle must resume when privilege drops to uninhibited mode";
  EXPECT_EQ(minstret->AsUint64(), start_instret + 2) << "minstret must resume when privilege drops to uninhibited mode";
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
