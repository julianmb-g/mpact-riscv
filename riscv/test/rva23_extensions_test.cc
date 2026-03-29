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
#include "utils/assembler/native_assembler_wrapper.h"
#include "absl/log/check.h"
#include "riscv/riscv64g_bin_encoder.h"

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
using ::mpact::sim::assembler::NativeTextualAssembler;

class Rva23ExtensionsTest : public ::testing::Test {
 protected:
  Rva23ExtensionsTest() {
    memory_ = new FlatDemandMemory();
    atomic_memory_ = new AtomicMemory(memory_);
    state_ = new RiscVState("test_rva23_extensions", RiscVXlen::RV64, memory_, atomic_memory_);
    
    fp_state_ = new RiscVFPState(state_->csr_set(), state_);
    state_->set_rv_fp(fp_state_);

    vector_state_ = new RiscVVectorState(state_, 64);
    state_->set_rv_vector(vector_state_);

    decoder_ = new Rva23u64DecoderWrapper(state_, memory_);

    std::string reg_name;
    for (int i = 0; i < 32; i++) {
      reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
      CHECK_NE(state_->AddRegister<RV64Register>(reg_name), nullptr);
      CHECK_OK(state_->AddRegisterAlias<RV64Register>(
          reg_name, ::mpact::sim::riscv::kXRegisterAliases[i]));
    }
    for (int i = 0; i < 32; i++) {
      reg_name = absl::StrCat(RiscVState::kFregPrefix, i);
      CHECK_NE(state_->AddRegister<RVFpRegister>(reg_name), nullptr);
      CHECK_OK(state_->AddRegisterAlias<RVFpRegister>(
          reg_name, ::mpact::sim::riscv::kFRegisterAliases[i]));
    }

    top_ = new RiscVTop("test_top", state_, decoder_);
    CHECK_OK(top_->WriteRegister("pc", 0x1000));
  }

  ~Rva23ExtensionsTest() override {
    delete top_;
    delete decoder_;
    delete vector_state_;
    delete fp_state_;
    delete state_;
    delete atomic_memory_;
    delete memory_;
  }

  void WriteInstruction(uint64_t address, uint32_t inst) {
    auto db = state_->db_factory()->Allocate<uint32_t>(1);
    db->Set<uint32_t>(0, inst);
    memory_->Store(address, db);
    db->DecRef();
  }

  FlatDemandMemory* memory_ = nullptr;
  AtomicMemory* atomic_memory_ = nullptr;
  RiscVState* state_ = nullptr;
  RiscVFPState* fp_state_ = nullptr;
  RiscVVectorState* vector_state_ = nullptr;
  Rva23u64DecoderWrapper* decoder_ = nullptr;
  RiscVTop* top_ = nullptr;
};

TEST_F(Rva23ExtensionsTest, AuthenticZfaExecutionFroundS) {
  // Test native E2E integration, proving decoder and execution boundary are linked.
  
  float initial_val = 2.5f;
  uint32_t val_bits;
  std::memcpy(&val_bits, &initial_val, sizeof(float));
  uint64_t nan_boxed = 0xffffffff00000000ULL | val_bits;

  auto write_status = top_->WriteRegister("f10", nan_boxed);
  ASSERT_TRUE(write_status.ok());

  // fround.s fa0, fa0, rmm -> 0x40454553
  NativeTextualAssembler assembler;
  auto fround_res = assembler.Assemble("fround.s fa0, fa0, rmm\n");
  ASSERT_TRUE(fround_res.ok()) << fround_res.status().message();
  ASSERT_EQ(fround_res.value().size(), 4);
  uint32_t fround_inst;
  std::memcpy(&fround_inst, fround_res.value().data(), sizeof(uint32_t));
  WriteInstruction(0x1000, fround_inst);
  
  auto step_status = top_->Step(1);
  EXPECT_TRUE(step_status.ok());

  // Wait, Zfa rounding RMM for 2.5f is 3.0f.
  auto read_result = top_->ReadRegister("f10");
  ASSERT_TRUE(read_result.ok());

  uint64_t res_val = read_result.value();
  uint32_t res_u32 = static_cast<uint32_t>(res_val);
  float res_float;
  std::memcpy(&res_float, &res_u32, sizeof(float));
  
  // RMM rounds half to max magnitude -> 3.0
  EXPECT_EQ(res_float, 3.0f);
  EXPECT_EQ(top_->ReadRegister("pc").value(), 0x1004);
}

TEST_F(Rva23ExtensionsTest, AuthenticZawrsExecutionWrsNto) {
  // wrs.nto -> 0x00d00073
  NativeTextualAssembler assembler;
  auto wrs_res = assembler.Assemble("wrs.nto\nnop\n");
  ASSERT_TRUE(wrs_res.ok()) << wrs_res.status().message();
  ASSERT_EQ(wrs_res.value().size(), 8);
  uint32_t wrs_inst, nop_inst;
  std::memcpy(&wrs_inst, wrs_res.value().data(), sizeof(uint32_t));
  std::memcpy(&nop_inst, wrs_res.value().data() + 4, sizeof(uint32_t));
  WriteInstruction(0x1000, wrs_inst);
  WriteInstruction(0x1004, nop_inst);
  
  auto step_status = top_->Step(1);
  EXPECT_TRUE(step_status.ok());
  
  // PC should advance past wrs.nto organically natively.
  EXPECT_EQ(top_->ReadRegister("pc").value(), 0x1004);
}

}  // namespace
