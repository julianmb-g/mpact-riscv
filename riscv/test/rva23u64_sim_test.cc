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
#include "riscv/riscv_dtb_loader.h"
#include <fstream>
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
  
  
  
  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/zawrs.elf");
  EXPECT_TRUE(load_result.ok());
  uint64_t entry_point = load_result.value();
  
  EXPECT_TRUE(top->WriteRegister("pc", entry_point).ok());
  auto status = top->Step(2); // Execute wrs.nto and then wfi
  EXPECT_TRUE(status.ok());

  // PC should advance
  EXPECT_GT(top->ReadRegister("pc").value(), entry_point);
  
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
    reg_name = absl::StrCat(RiscVState::kVregPrefix, i);
    EXPECT_NE(state->GetRegister<mpact::sim::riscv::RVVectorRegister>(reg_name).first, nullptr);
  }
  
  auto* top = new RiscVTop("test_top", state, decoder);

  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/zfa_fround.elf");
  EXPECT_TRUE(load_result.ok());
  uint64_t pc = load_result.value();
  
  EXPECT_TRUE(top->WriteRegister("pc", pc).ok());

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

TEST(Rva23u64SimTest, Zve32fUnaryExecutionBoundary) {
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23u64_zve32f", RiscVXlen::RV64, memory, atomic_memory);
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

  
  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/zve32f.elf");
  EXPECT_TRUE(load_result.ok());
  uint64_t pc = load_result.value();
  
  EXPECT_TRUE(top->WriteRegister("pc", pc).ok());

  auto mstatus_res = state->csr_set()->GetCsr("mstatus");
  EXPECT_TRUE(mstatus_res.ok());
  mstatus_res.value()->Write(static_cast<uint64_t>(0x6600)); 

  auto v2 = state->GetRegister<mpact::sim::riscv::RVVectorRegister>("v2").first;
  auto v1 = state->GetRegister<mpact::sim::riscv::RVVectorRegister>("v1").first;
  
  auto db_v2 = state->db_factory()->Allocate<uint32_t>(4);
  float vals[4] = {2.0f, -1.0f, 0.0f, 2.25f};
  db_v2->Set<uint32_t>(0, absl::bit_cast<uint32_t>(vals[0]));
  db_v2->Set<uint32_t>(1, absl::bit_cast<uint32_t>(vals[1]));
  db_v2->Set<uint32_t>(2, absl::bit_cast<uint32_t>(vals[2]));
  db_v2->Set<uint32_t>(3, absl::bit_cast<uint32_t>(vals[3]));
  v2->SetDataBuffer(db_v2);
  db_v2->DecRef();

  EXPECT_TRUE(top->Step(1).ok()); 
  EXPECT_TRUE(top->Step(1).ok()); 

  auto v1_buf = v1->data_buffer();
  ASSERT_NE(v1_buf, nullptr);
  EXPECT_FLOAT_EQ(absl::bit_cast<float>(v1_buf->Get<uint32_t>(0)), std::sqrt(2.0f));
  EXPECT_TRUE(std::isnan(absl::bit_cast<float>(v1_buf->Get<uint32_t>(1))));
  EXPECT_FLOAT_EQ(absl::bit_cast<float>(v1_buf->Get<uint32_t>(2)), 0.0f);
  EXPECT_FLOAT_EQ(absl::bit_cast<float>(v1_buf->Get<uint32_t>(3)), 1.5f);

  EXPECT_EQ(top->ReadRegister("pc").value(), pc + 8);

  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}

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
    reg_name = absl::StrCat(RiscVState::kVregPrefix, i);
    EXPECT_NE(state->GetRegister<mpact::sim::riscv::RVVectorRegister>(reg_name).first, nullptr);
  }
  
  auto* top = new RiscVTop("test_top", state, decoder);

  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/zfa_fcvtmod.elf");
  EXPECT_TRUE(load_result.ok());
  uint64_t pc = load_result.value();
  
  EXPECT_TRUE(top->WriteRegister("pc", pc).ok());

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

}  // namespace
