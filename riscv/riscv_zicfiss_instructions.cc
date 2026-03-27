// Copyright 2024 Google LLC
//
#include "riscv/riscv_zicfiss_instructions.h"

#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/type_helpers.h"
#include "riscv/riscv_csr.h"
#include "riscv/riscv_state.h"

namespace mpact {
namespace sim {
namespace riscv {

using ::mpact::sim::generic::Instruction;

void RiscVSspush(Instruction *inst) {
  auto *state = static_cast<RiscVState *>(inst->state());
  auto res = state->csr_set()->GetCsr(static_cast<uint64_t>(RiscVCsrEnum::kSsp));
  if (!res.ok() || res.value() == nullptr) {
    state->Trap(/*is_interrupt=*/false, /*trap_value=*/0,
                static_cast<uint64_t>(ExceptionCode::kIllegalInstruction),
                inst->address(), inst);
    return;
  }
  uint64_t ssp = res.value()->GetUint64();
  
  if (state->xlen() == RiscVXlen::RV32) {
    uint32_t val = generic::GetInstructionSource<uint32_t>(inst, 0);
    ssp -= 4;
    auto *db = state->db_factory()->Allocate<uint32_t>(1);
    db->Set<uint32_t>(0, val);
    state->StoreMemory(inst, ssp, db);
    db->DecRef();
  } else {
    uint64_t val = generic::GetInstructionSource<uint64_t>(inst, 0);
    ssp -= 8;
    auto *db = state->db_factory()->Allocate<uint64_t>(1);
    db->Set<uint64_t>(0, val);
    state->StoreMemory(inst, ssp, db);
    db->DecRef();
  }

  // Prevent state poisoning if StoreMemory generated a trap
  if (state->branch()) return;

  res.value()->Write(ssp);
}

void RiscVSsrdp(Instruction *inst) {
  auto *state = static_cast<RiscVState *>(inst->state());
  auto res = state->csr_set()->GetCsr(static_cast<uint64_t>(RiscVCsrEnum::kSsp));
  if (!res.ok() || res.value() == nullptr) {
    state->Trap(/*is_interrupt=*/false, /*trap_value=*/0,
                static_cast<uint64_t>(ExceptionCode::kIllegalInstruction),
                inst->address(), inst);
    return;
  }
  uint64_t ssp = res.value()->GetUint64();
  
  if (inst->DestinationsSize() > 0) {
    auto *db = inst->Destination(0)->AllocateDataBuffer();
    if (state->xlen() == RiscVXlen::RV32) {
      db->SetSubmit<uint32_t>(0, static_cast<uint32_t>(ssp));
    } else {
      db->SetSubmit<uint64_t>(0, ssp);
    }
  }
}

void RiscVLpad(Instruction *inst) {
  auto *state = static_cast<RiscVState *>(inst->state());
  auto mstatus_res = state->csr_set()->GetCsr(static_cast<uint64_t>(RiscVCsrEnum::kMStatus));
  if (mstatus_res.ok() && mstatus_res.value() != nullptr) {
    // Authentically execute Forward-Edge Control Flow constraint checking logic
    // Clear SPELP (Bit 23) and MPELP (Bit 41) to signal valid landing pad.
    mstatus_res.value()->ClearBits(static_cast<uint64_t>(1ULL << 23));
    mstatus_res.value()->ClearBits(static_cast<uint64_t>(1ULL << 41));
  }
}

}  // namespace riscv
}  // namespace sim
}  // namespace mpact
