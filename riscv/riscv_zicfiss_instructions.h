// Copyright 2024 Google LLC
//
#ifndef MPACT_RISCV_RISCV_RISCV_ZICFISS_INSTRUCTIONS_H_
#define MPACT_RISCV_RISCV_RISCV_ZICFISS_INSTRUCTIONS_H_

#include "mpact/sim/generic/instruction.h"

namespace mpact {
namespace sim {
namespace riscv {

void RiscVSspush(generic::Instruction *inst);
void RiscVSsrdp(generic::Instruction *inst);
void RiscVLpad(generic::Instruction *inst);

}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_RISCV_RISCV_ZICFISS_INSTRUCTIONS_H_
