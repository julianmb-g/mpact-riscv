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
#include "gmock/gmock.h"
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
#include "riscv/riscv_clint.h"
#include <functional>

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
using ::mpact::sim::riscv::RVFpRegister;
using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::util::AtomicMemory;
using ::mpact::sim::riscv::STimeCmpCsr;

class RiscVSstcTest : public ::testing::Test {
 public:
  RiscVSstcTest() {
    memory_ = new FlatDemandMemory();
    atomic_memory_ = new AtomicMemory(memory_);
    state_ = new RiscVState("RVA23S64", RiscVXlen::RV64, memory_, atomic_memory_);
    
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

    clint_ = new RiscVClint(1, state_->mip());
  }

  ~RiscVSstcTest() override {
    delete clint_;
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
  RiscVClint* clint_;
};

TEST_F(RiscVSstcTest, TestSstcStimecmpInterrupt) {
  uint64_t inst_addr = 0x1000;
  
  // csrw stimecmp, x5 -> 0x14D29073
  uint32_t inst_csrw = 0x14D29073;
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, inst_csrw);
  memory_->Store(inst_addr, db);
  db->DecRef();

  // NOPs (addi x0, x0, 0) for subsequent execution sequence
  uint32_t inst_nop = 0x00000013;
  for (int i = 1; i <= 5; ++i) {
    db = state_->db_factory()->Allocate<uint32_t>(1);
    db->Set<uint32_t>(0, inst_nop);
    memory_->Store(inst_addr + i * 4, db);
    db->DecRef();
  }

  // Simulate an advancing hardware clock external to the architectural core
  uint64_t simulated_clock = 5; // Fixed external clock
  
  auto csr_res = state_->csr_set()->GetCsr(0x14D); // stimecmp
  ASSERT_TRUE(csr_res.ok());
  auto* stimecmp = static_cast<STimeCmpCsr*>(csr_res.value());
  
  // The architectural wire from the out-of-band hardware timer to the Sstc interface
  // When stimecmp is written, this callback checks against the external time.
  stimecmp->set_timer_cb([&](uint64_t val) {
    if (simulated_clock >= val) {
      state_->mip()->SetBits(static_cast<uint64_t>(0x20)); // Set STIP
    } else {
      state_->mip()->ClearBits(static_cast<uint64_t>(0x20));
    }
  });

  // Architectural Configuration
  state_->set_privilege_mode(PrivilegeMode::kSupervisor);
  
  auto sstatus_res = state_->csr_set()->GetCsr(0x100);
  ASSERT_TRUE(sstatus_res.ok());
  sstatus_res.value()->SetBits(static_cast<uint64_t>(0x2)); // sstatus.SIE = 1

  auto sie_res = state_->csr_set()->GetCsr(0x104);
  ASSERT_TRUE(sie_res.ok());
  sie_res.value()->SetBits(static_cast<uint64_t>(0x20)); // sie.STIE = 1

  auto mideleg_res = state_->csr_set()->GetCsr(0x303);
  ASSERT_TRUE(mideleg_res.ok());
  mideleg_res.value()->SetBits(static_cast<uint64_t>(0x20)); // mideleg.STIP = 1

  uint64_t trap_vector = 0x2000;
  auto stvec_res = state_->csr_set()->GetCsr(0x105);
  ASSERT_TRUE(stvec_res.ok());
  stvec_res.value()->Write(trap_vector);

  // We write `3` to stimecmp. Since simulated_clock is 5, the interrupt should fire immediately
  // upon the retirement of the csrw instruction.
  auto t0_write = top_->WriteRegister("x5", 3ULL);
  EXPECT_TRUE(t0_write.ok());

  auto pc_write = top_->WriteRegister("pc", inst_addr);
  EXPECT_TRUE(pc_write.ok());

  // Execute csrw
  auto status = top_->Step(1);
  EXPECT_TRUE(status.ok());
  
  // Natively evaluated at retirement! The PC should jump directly to the trap vector.
  EXPECT_EQ(top_->ReadRegister("pc").value(), trap_vector) << "Trap not taken at bounds!";

  // Verify Architectural Vector Output
  auto scause_res = state_->csr_set()->GetCsr(0x142); // scause
  ASSERT_TRUE(scause_res.ok());
  EXPECT_EQ(scause_res.value()->AsUint64(), (1ULL << 63) | 5ULL);
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
