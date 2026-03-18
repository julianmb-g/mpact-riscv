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

void RiscVCboInval(Instruction* inst) {
  // Cache block invalidation. For functional simulation, this is a no-op.
}

void RiscVCboClean(Instruction* inst) {
  // Cache block clean. For functional simulation, this is a no-op.
}

void RiscVCboFlush(Instruction* inst) {
  // Cache block flush. For functional simulation, this is a no-op.
}

void RiscVCboZero(Instruction* inst) {
  auto* state = static_cast<RiscVState*>(inst->state());

  // Privilege checks for Zicboz
  bool cbze_enabled = true;
  auto priv_mode = state->privilege_mode();
  if (priv_mode != PrivilegeMode::kMachine) {
    auto res_m = state->csr_set()->GetCsr(static_cast<uint64_t>(RiscVCsrEnum::kMenvcfg));
    if (res_m.ok() && res_m.value() != nullptr) {
      uint64_t menvcfg = res_m.value()->GetUint64();
      if ((menvcfg & (1ULL << 4)) == 0) {
        cbze_enabled = false;
      }
    }
  }
  if (cbze_enabled && priv_mode == PrivilegeMode::kUser) {
    auto res_s = state->csr_set()->GetCsr(static_cast<uint64_t>(RiscVCsrEnum::kSenvcfg));
    if (res_s.ok() && res_s.value() != nullptr) {
      uint64_t senvcfg = res_s.value()->GetUint64();
      if ((senvcfg & (1ULL << 4)) == 0) {
        cbze_enabled = false;
      }
    }
  }

  if (!cbze_enabled) {
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
