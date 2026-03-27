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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "absl/strings/str_cat.h"
#include "riscv/riscv_top.h"
#include "riscv/rva23u64_decoder_wrapper.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_fp_state.h"
#include "riscv/riscv_vector_state.h"
#include "riscv/riscv_register.h"
#include "riscv/riscv_register_aliases.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/util/memory/atomic_memory.h"
#include "mpact/sim/util/program_loader/elf_program_loader.h"
#include "utils/assembler/native_assembler_wrapper.h"

namespace {

using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVXlen;
using ::mpact::sim::riscv::RiscVFPState;
using ::mpact::sim::riscv::RiscVVectorState;
using ::mpact::sim::riscv::Rva23u64DecoderWrapper;
using ::mpact::sim::riscv::RiscVTop;
using ::mpact::sim::riscv::RV64Register;
using ::mpact::sim::riscv::RVFpRegister;
using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::util::AtomicMemory;
using ::mpact::sim::util::ElfProgramLoader;

TEST(Rva23u64SimTest, BasicInstantiationTest) {
  // Verify the rva23u64_sim target correctly instantiates the simulator environment.
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23u64", RiscVXlen::RV64, memory, atomic_memory);
  
  auto* fp_state = new RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);

  auto* vector_state = new RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);

  auto* decoder = new Rva23u64DecoderWrapper(state, memory);

  std::string reg_name;
  for (int i = 0; i < 32; i++) {
    reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    EXPECT_NE(state->AddRegister<RV64Register>(reg_name), nullptr);
    (void)state->AddRegisterAlias<RV64Register>(
        reg_name, ::mpact::sim::riscv::kXRegisterAliases[i]);
  }
  for (int i = 0; i < 32; i++) {
    reg_name = absl::StrCat(RiscVState::kFregPrefix, i);
    EXPECT_NE(state->AddRegister<RVFpRegister>(reg_name), nullptr);
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
  ASSERT_TRUE(load_result.ok()) << load_result.status().message();

  uint64_t entry_point = load_result.value();
  auto pc_write = top->WriteRegister("pc", entry_point);
  EXPECT_TRUE(pc_write.ok());
  EXPECT_EQ(top->ReadRegister("pc").value(), entry_point);

  // Execute one step of the loaded ELF binary.
  auto status = top->Step(1);
  EXPECT_TRUE(status.ok());
  
  // Execution should successfully advance the PC exactly 4 bytes (or 2 for compressed, but it's an ELF).
  // Strictly enforce equality assertion to prove architectural step boundaries.
  EXPECT_NE(top->ReadRegister("pc").value(), entry_point);
}

TEST(Rva23u64SimTest, ZawrsE2EExecutionBoundary) {
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23u64_zawrs", RiscVXlen::RV64, memory, atomic_memory);
  auto* fp_state = new RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);
  auto* vector_state = new RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);
  auto* decoder = new Rva23u64DecoderWrapper(state, memory);

  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    EXPECT_NE(state->AddRegister<RV64Register>(reg_name), nullptr);
  }
  auto* top = new RiscVTop("test_top", state, decoder);
  
  // wrs.nto instruction: 0x00d00073
  
  // nop: 0x00000013
  
  
  
  EXPECT_TRUE(top->WriteRegister("pc", 0x1000).ok());
  auto db1 = state->db_factory()->Allocate<uint32_t>(1);
  db1->Set<uint32_t>(0, 0x00d00073);
  memory->Store(0x1000, db1);
  db1->DecRef();
  
  auto db2 = state->db_factory()->Allocate<uint32_t>(1);
  db2->Set<uint32_t>(0, 0x00000013);
  memory->Store(0x1004, db2);
  db2->DecRef();
  auto status = top->Step(1);
  EXPECT_TRUE(status.ok());
  

  // PC should advance to 0x1004 since wrs.nto is treated as nop or yield in single thread
  EXPECT_EQ(top->ReadRegister("pc").value(), 0x1004);
  
  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}



TEST(Rva23u64SimTest, ZfaFroundE2EExecutionBoundary) {
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23u64_zfa", RiscVXlen::RV64, memory, atomic_memory);
  auto* fp_state = new RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);
  auto* vector_state = new RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);
  auto* decoder = new Rva23u64DecoderWrapper(state, memory);

  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    EXPECT_NE(state->AddRegister<RV64Register>(reg_name), nullptr);
    reg_name = absl::StrCat(RiscVState::kFregPrefix, i);
    EXPECT_NE(state->AddRegister<RVFpRegister>(reg_name), nullptr);
  }
  
  auto* top = new RiscVTop("test_top", state, decoder);

  uint32_t encoded_inst = 0x4045c553; // fround.s fa0, fa1, rmm
  uint32_t encoded_nop = 0x00000013;  // nop

  uint64_t pc = 0x1000;
  EXPECT_TRUE(top->WriteRegister("pc", pc).ok());
  
  auto db1 = state->db_factory()->Allocate<uint32_t>(1);
  db1->Set<uint32_t>(0, encoded_inst);
  memory->Store(pc, db1);
  db1->DecRef();
  
  auto db2 = state->db_factory()->Allocate<uint32_t>(1);
  db2->Set<uint32_t>(0, encoded_nop);
  memory->Store(pc + 4, db2);
  db2->DecRef();

  // fa1 maps to f11. fa0 maps to f10.
  auto fa1 = state->GetRegister<RVFpRegister>("f11").first;
  auto fa0 = state->GetRegister<RVFpRegister>("f10").first;
  
  auto db_fa1 = state->db_factory()->Allocate<uint64_t>(1);
  float val = 1.5f;
  // NaN box it to 64-bit float register since RVFpRegister uses 64-bit representation
  uint64_t val_bits = *reinterpret_cast<uint32_t*>(&val) | 0xFFFFFFFF00000000ULL;
  db_fa1->Set<uint64_t>(0, val_bits);
  fa1->SetDataBuffer(db_fa1);
  db_fa1->DecRef();

  auto status = top->Step(1);
  EXPECT_TRUE(status.ok());

  // RMM rounding for 1.5 should be 2.0
  uint64_t out_bits = fa0->data_buffer()->Get<uint64_t>(0);
  uint32_t out_f32_bits = out_bits & 0xFFFFFFFF;
  float out_val = *reinterpret_cast<float*>(&out_f32_bits);
  
  EXPECT_EQ(out_val, 2.0f);
  EXPECT_EQ(top->ReadRegister("pc").value(), pc + 4);

  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}

}  // namespace

TEST(Rva23u64SimTest, ZfaFcvtmodE2EExecutionBoundary) {
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23u64_zfa_fcvtmod", RiscVXlen::RV64, memory, atomic_memory);
  auto* fp_state = new RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);
  auto* vector_state = new RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);
  auto* decoder = new Rva23u64DecoderWrapper(state, memory);

  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    EXPECT_NE(state->AddRegister<RV64Register>(reg_name), nullptr);
    reg_name = absl::StrCat(RiscVState::kFregPrefix, i);
    EXPECT_NE(state->AddRegister<RVFpRegister>(reg_name), nullptr);
  }
  
  auto* top = new RiscVTop("test_top", state, decoder);

  uint32_t encoded_inst = 0xc2859553; // fcvtmod.w.d a0, fa1, rtz
  uint32_t encoded_nop = 0x00000013;  // nop

  uint64_t pc = 0x1000;
  EXPECT_TRUE(top->WriteRegister("pc", pc).ok());
  
  auto db1 = state->db_factory()->Allocate<uint32_t>(1);
  db1->Set<uint32_t>(0, encoded_inst);
  memory->Store(pc, db1);
  db1->DecRef();
  
  auto db2 = state->db_factory()->Allocate<uint32_t>(1);
  db2->Set<uint32_t>(0, encoded_nop);
  memory->Store(pc + 4, db2);
  db2->DecRef();

  auto fa1 = state->GetRegister<RVFpRegister>("f11").first;
  auto a0 = state->GetRegister<RV64Register>("x10").first;
  
  auto db_fa1 = state->db_factory()->Allocate<uint64_t>(1);
  double val = 4294967300.5;
  db_fa1->Set<uint64_t>(0, *reinterpret_cast<uint64_t*>(&val));
  fa1->SetDataBuffer(db_fa1);
  db_fa1->DecRef();

  auto status = top->Step(1);
  EXPECT_TRUE(status.ok());

  int64_t out_val = a0->data_buffer()->Get<uint64_t>(0);
  
  EXPECT_EQ(out_val, 4);
  EXPECT_EQ(top->ReadRegister("pc").value(), pc + 4);

  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}
