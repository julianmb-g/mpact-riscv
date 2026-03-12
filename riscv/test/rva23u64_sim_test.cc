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

namespace {

using ::mpact::sim::riscv::RiscVTop;
using ::mpact::sim::riscv::Rva23u64DecoderWrapper;
using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVFPState;
using ::mpact::sim::riscv::RiscVVectorState;
using ::mpact::sim::riscv::RiscVXlen;
using ::mpact::sim::riscv::RV64Register;
using ::mpact::sim::riscv::RVFpRegister;
using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::util::AtomicMemory;

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

  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}

} // namespace
