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
  // We want to organically execute: csrw stimecmp, t0
  // t0 is x5. The instruction bytes for `csrw stimecmp, x5` is 0x14D29073.
  uint32_t inst_csrw = 0x14D29073;

  uint64_t inst_addr = 0x1000;
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, inst_csrw);
  memory_->Store(inst_addr, db);
  db->DecRef();

  // Set the mock "current time" to 500
  uint64_t current_time = 500;
  
  auto csr_res = state_->csr_set()->GetCsr(0x14D); // stimecmp
  ASSERT_TRUE(csr_res.ok());
  auto* stimecmp = static_cast<STimeCmpCsr*>(csr_res.value());
  
  // Wire the stimecmp callback to our hardware comparator simulator
  stimecmp->set_timer_cb([&](uint64_t val) {
    if (current_time >= val) {
      // Pend the Supervisor Timer Interrupt (bit 5)
      state_->mip()->SetBits(static_cast<uint64_t>(0x20));
      state_->CheckForInterrupt();
    } else {
      state_->mip()->ClearBits(static_cast<uint64_t>(0x20));
    }
  });

  // Configure architectural state to allow S-mode timer interrupts
  state_->set_privilege_mode(PrivilegeMode::kSupervisor);
  
  // sstatus.SIE is bit 1
  auto sstatus_res = state_->csr_set()->GetCsr(0x100); // sstatus
  ASSERT_TRUE(sstatus_res.ok());
  sstatus_res.value()->SetBits(static_cast<uint64_t>(0x2));

  // sie.STIE is bit 5
  auto sie_res = state_->csr_set()->GetCsr(0x104); // sie
  ASSERT_TRUE(sie_res.ok());
  sie_res.value()->SetBits(static_cast<uint64_t>(0x20));

  // Delegate STIP to S-mode by setting mideleg bit 5
  auto mideleg_res = state_->csr_set()->GetCsr(0x303); // mideleg
  ASSERT_TRUE(mideleg_res.ok());
  mideleg_res.value()->SetBits(static_cast<uint64_t>(0x20));

  // set stvec to 0x2000 (our trap handler)
  uint64_t trap_vector = 0x2000;
  auto stvec_res = state_->csr_set()->GetCsr(0x105); // stvec
  ASSERT_TRUE(stvec_res.ok());
  stvec_res.value()->Write(trap_vector);

  // set t0 (x5) to 500 (so that current_time >= stimecmp triggers the interrupt)
  auto t0_write = top_->WriteRegister("x5", 500ULL);
  EXPECT_TRUE(t0_write.ok());

  // Set PC
  auto pc_write = top_->WriteRegister("pc", inst_addr);
  EXPECT_TRUE(pc_write.ok());

  // Step 1: Execute `csrw stimecmp, t0`. This should decode, execute, and write 500 to stimecmp.
  // The callback fires, sees current_time (500) >= stimecmp (500), and sets mip.STIP = 1.
  auto status = top_->Step(1);
  EXPECT_TRUE(status.ok());

  // The instruction retired successfully, and because STIP became pending and enabled,
  // the core immediately evaluated CheckForInterrupt() and vectored to the S-mode trap handler.
  EXPECT_EQ(top_->ReadRegister("pc").value(), trap_vector);

  // Verify scause is 5 (Supervisor Timer Interrupt) with the Interrupt bit (63) set.
  auto scause_res = state_->csr_set()->GetCsr(0x142); // scause
  ASSERT_TRUE(scause_res.ok());
  EXPECT_EQ(scause_res.value()->AsUint64(), (1ULL << 63) | 5ULL);
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
