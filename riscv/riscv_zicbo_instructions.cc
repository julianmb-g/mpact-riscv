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
#include <vector>

#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/type_helpers.h"
#include "riscv/riscv_csr.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_instruction_helpers.h"

namespace mpact::sim::riscv {

using ::mpact::sim::generic::Instruction;

// Default cache block size if not specified.
constexpr uint64_t kDefaultCacheBlockSize = 64;

static bool CheckZicbomPrivilege(RiscVState* state, Instruction* inst, bool is_inval) {
  auto priv_mode = state->privilege_mode();
  if (priv_mode == PrivilegeMode::kMachine) return true;

  auto get_cbie = [](uint64_t envcfg) { return (envcfg >> 4) & 3; };
  auto get_cbcfe = [](uint64_t envcfg) { return (envcfg >> 6) & 1; };

  uint64_t menvcfg = 0;
  auto res_m = state->csr_set()->GetCsr(static_cast<uint64_t>(RiscVCsrEnum::kMenvcfg));
  if (res_m.ok() && res_m.value() != nullptr) {
    menvcfg = res_m.value()->GetUint64();
  }

  if (is_inval) {
    uint64_t m_cbie = get_cbie(menvcfg);
    if (m_cbie == 0 || m_cbie == 2) return false;
  } else {
    if (get_cbcfe(menvcfg) == 0) return false;
  }

  if (priv_mode == PrivilegeMode::kUser) {
    uint64_t senvcfg = 0;
    auto res_s = state->csr_set()->GetCsr(static_cast<uint64_t>(RiscVCsrEnum::kSenvcfg));
    if (res_s.ok() && res_s.value() != nullptr) {
      senvcfg = res_s.value()->GetUint64();
    }
    if (is_inval) {
      uint64_t s_cbie = get_cbie(senvcfg);
      if (s_cbie == 0 || s_cbie == 2) return false;
    } else {
      if (get_cbcfe(senvcfg) == 0) return false;
    }
  }

  return true;
}

static bool CheckZicbozPrivilege(RiscVState* state, Instruction* inst) {
  auto priv_mode = state->privilege_mode();
  if (priv_mode == PrivilegeMode::kMachine) return true;

  // CBZE is bit 7 in menvcfg and senvcfg
  auto get_cbze = [](uint64_t envcfg) { return (envcfg >> 7) & 1; };

  uint64_t menvcfg = 0;
  auto res_m = state->csr_set()->GetCsr(static_cast<uint64_t>(RiscVCsrEnum::kMenvcfg));
  if (res_m.ok() && res_m.value() != nullptr) {
    menvcfg = res_m.value()->GetUint64();
  }

  if (get_cbze(menvcfg) == 0) return false;

  if (priv_mode == PrivilegeMode::kUser) {
    uint64_t senvcfg = 0;
    auto res_s = state->csr_set()->GetCsr(static_cast<uint64_t>(RiscVCsrEnum::kSenvcfg));
    if (res_s.ok() && res_s.value() != nullptr) {
      senvcfg = res_s.value()->GetUint64();
    }
    if (get_cbze(senvcfg) == 0) return false;
  }

  return true;
}

void RiscVCboInval(Instruction* inst) {
  auto* state = static_cast<RiscVState*>(inst->state());
  if (!CheckZicbomPrivilege(state, inst, /*is_inval=*/true)) {
    state->Trap(/*is_interrupt=*/false, /*trap_value=*/0,
                static_cast<uint64_t>(ExceptionCode::kIllegalInstruction),
                inst->address(), inst);
    return;
  }
  // Cache block invalidation. For functional simulation, this is a no-op if authorized.
}

void RiscVCboClean(Instruction* inst) {
  auto* state = static_cast<RiscVState*>(inst->state());
  if (!CheckZicbomPrivilege(state, inst, /*is_inval=*/false)) {
    state->Trap(/*is_interrupt=*/false, /*trap_value=*/0,
                static_cast<uint64_t>(ExceptionCode::kIllegalInstruction),
                inst->address(), inst);
    return;
  }
  // Cache block clean. For functional simulation, this is a no-op if authorized.
}

void RiscVCboFlush(Instruction* inst) {
  auto* state = static_cast<RiscVState*>(inst->state());
  if (!CheckZicbomPrivilege(state, inst, /*is_inval=*/false)) {
    state->Trap(/*is_interrupt=*/false, /*trap_value=*/0,
                static_cast<uint64_t>(ExceptionCode::kIllegalInstruction),
                inst->address(), inst);
    return;
  }
  // Cache block flush. For functional simulation, this is a no-op if authorized.
}

void RiscVCboZero(Instruction* inst) {
  auto* state = static_cast<RiscVState*>(inst->state());

  if (!CheckZicbozPrivilege(state, inst)) {
    state->Trap(/*is_interrupt=*/false, /*trap_value=*/0,
                static_cast<uint64_t>(ExceptionCode::kIllegalInstruction),
                inst->address(), inst);
    return;
  }

  // Cache block zero. Must write zeros to the block of memory.
  auto* db = state->db_factory()->Allocate<uint8_t>(kDefaultCacheBlockSize);
  
  // Fill the data buffer with zeros.
  auto data_span = db->Get<uint8_t>();
  for (size_t i = 0; i < kDefaultCacheBlockSize; ++i) {
    data_span[i] = 0;
  }

  // The address is in rs1.
  uint64_t addr = mpact::sim::generic::GetInstructionSource<uint64_t>(inst, 0);
  
  // Align address to the cache block size.
  addr &= ~(kDefaultCacheBlockSize - 1);

  // Store the zeros into memory.
  state->StoreMemory(inst, addr, db);
  
  db->DecRef();
}

}  // namespace mpact::sim::riscv
