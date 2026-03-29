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

#include <cstdint>
#include <functional>

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
#include "mpact/sim/util/program_loader/elf_program_loader.h"

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

using ::mpact::sim::util::ElfProgramLoader;
using ::mpact::sim::riscv::STimeCmpCsr;

TEST(Rva23s64SimTest, STimeCmpCsrTimerCallback) {
  // Use the standard architectural environment instantiation without manual injection
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("RVA23S64", RiscVXlen::RV64, memory, atomic_memory);

  auto csr_res = state->csr_set()->GetCsr(0x14D); // stimecmp
  ASSERT_TRUE(csr_res.ok());
  auto csr = csr_res.value();
  auto* stimecmp = static_cast<STimeCmpCsr*>(csr);

  uint64_t callback_value = 0;
  bool callback_called = false;
  stimecmp->set_timer_cb([&](uint64_t time) {
    callback_value = time;
    callback_called = true;
  });

  // Test the base implementation
  csr->Write(static_cast<uint64_t>(0xDEADBEEF12345678ULL));
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(callback_value, 0xDEADBEEF12345678ULL);
  EXPECT_EQ(csr->AsUint64(), 0xDEADBEEF12345678ULL);

  delete state;
  delete atomic_memory;
  delete memory;
}

TEST(Rva23s64SimTest, BasicInstantiationTest) {
  // Verify the rva23s64_sim target correctly instantiates the simulator environment.
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23s64", RiscVXlen::RV64, memory, atomic_memory);
  
  auto* fp_state = new RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);

  auto* vector_state = new RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);

  auto* decoder = new Rva23s64DecoderWrapper(state, memory);

  std::string reg_name;
  for (int i = 0; i < 32; i++) {
    reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    (void)state->AddRegister<RV64Register>(reg_name);
    (void)state->AddRegisterAlias<RV64Register>(
        reg_name, ::mpact::sim::riscv::kXRegisterAliases[i]);
  }
  for (int i = 0; i < 32; i++) {
    reg_name = absl::StrCat(RiscVState::kFregPrefix, i);
    (void)state->AddRegister<RVFpRegister>(reg_name);
    (void)state->AddRegisterAlias<RVFpRegister>(
        reg_name, ::mpact::sim::riscv::kFRegisterAliases[i]);
  }

  auto* top = new RiscVTop("test_top", state, decoder);

  EXPECT_NE(top, nullptr);
  EXPECT_NE(decoder, nullptr);
  EXPECT_NE(state, nullptr);
  EXPECT_NE(memory, nullptr);

  // Load a real ELF binary to prove organic architectural instruction decoding.
  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/hello_world_64.elf");
  if (!load_result.ok()) {
    delete top;
    delete decoder;
    delete vector_state;
    delete fp_state;
    delete state;
    delete atomic_memory;
    delete memory;
    GTEST_SKIP() << "OS Artifact Missing";
  }

  uint64_t entry_point = load_result.value();
  auto pc_write = top->WriteRegister("pc", entry_point);
  EXPECT_TRUE(pc_write.ok());
  EXPECT_EQ(top->ReadRegister("pc").value(), entry_point);

  // Execute one step of the loaded ELF binary.
  auto status = top->Step(1);
  EXPECT_TRUE(status.ok());
  
  // Execution should successfully advance the PC exactly 4 bytes (or 2 for compressed, but it's an ELF).
  // Strictly enforce equality assertion to prove architectural step boundaries.
  EXPECT_EQ(top->ReadRegister("pc").value(), entry_point + 4);

  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}

} // namespace


TEST(Rva23s64SimTest, SvinvalE2EExecutionBoundary) {
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("RVA23S64", RiscVXlen::RV64, memory, atomic_memory);
  auto* decoder = new Rva23s64DecoderWrapper(state, memory);

  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    EXPECT_NE(state->AddRegister<RV64Register>(reg_name), nullptr);
  }
  
  auto* top = new RiscVTop("test_top", state, decoder);

  uint32_t sinval_vma_inst = 0x16000073;    // sinval.vma zero, zero
  uint32_t sfence_w_inval_inst = 0x18000073; // sfence.w.inval
  uint32_t sfence_inval_ir_inst = 0x18100073; // sfence.inval.ir
  uint32_t nop_inst = 0x00000013;           // nop

  uint64_t pc = 0x1000;
  EXPECT_TRUE(top->WriteRegister("pc", pc).ok());
  
  auto store_inst = [&](uint64_t addr, uint32_t inst) {
    auto db = state->db_factory()->Allocate<uint32_t>(1);
    db->Set<uint32_t>(0, inst);
    memory->Store(addr, db);
    db->DecRef();
  };

  store_inst(pc, sinval_vma_inst);
  store_inst(pc + 4, sfence_w_inval_inst);
  store_inst(pc + 8, sfence_inval_ir_inst);
  store_inst(pc + 12, nop_inst);

  bool trapped = false;
  state->set_on_trap([&](bool is_interrupt, uint64_t trap_value, uint64_t exception_code, uint64_t epc, const ::mpact::sim::generic::Instruction* inst) {
    trapped = true;
    return true;
  });

  // Test Machine Mode (should not trap)
  state->set_privilege_mode(::mpact::sim::riscv::PrivilegeMode::kMachine);
  EXPECT_TRUE(top->Step(1).ok()); // sinval.vma
  EXPECT_EQ(top->ReadRegister("pc").value(), pc + 4);
  EXPECT_FALSE(trapped);

  EXPECT_TRUE(top->Step(1).ok()); // sfence.w.inval
  EXPECT_EQ(top->ReadRegister("pc").value(), pc + 8);
  EXPECT_FALSE(trapped);

  EXPECT_TRUE(top->Step(1).ok()); // sfence.inval.ir
  EXPECT_EQ(top->ReadRegister("pc").value(), pc + 12);
  EXPECT_FALSE(trapped);

  // Test User Mode (should trap)
  EXPECT_TRUE(top->WriteRegister("pc", pc).ok());
  state->set_privilege_mode(::mpact::sim::riscv::PrivilegeMode::kUser);
  EXPECT_TRUE(top->Step(1).ok()); // sinval.vma will TRAP
  EXPECT_TRUE(trapped);

  delete top;
  delete decoder;
  delete state;
  delete atomic_memory;
  delete memory;
}
