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

#ifndef MPACT_RISCV_RISCV_RISCV_ZFA_INSTRUCTIONS_H_
#define MPACT_RISCV_RISCV_RISCV_ZFA_INSTRUCTIONS_H_

#include "mpact/sim/generic/instruction.h"

namespace mpact {
namespace sim {
namespace riscv {

using generic::Instruction;

// Single precision Zfa instructions.
void RiscVFliS(const Instruction* instruction);
void RiscVFMinmS(const Instruction* instruction);
void RiscVFMaxmS(const Instruction* instruction);
void RiscVFRoundS(const Instruction* instruction);
void RiscVFRoundnxS(const Instruction* instruction);
void RiscVFleqS(const Instruction* instruction);
void RiscVFltqS(const Instruction* instruction);

// Double precision Zfa instructions.
void RiscVFliD(const Instruction* instruction);
void RiscVFMinmD(const Instruction* instruction);
void RiscVFMaxmD(const Instruction* instruction);
void RiscVFRoundD(const Instruction* instruction);
void RiscVFRoundnxD(const Instruction* instruction);
void RiscVFCvtmodWD(const Instruction* instruction);
void RiscVFleqD(const Instruction* instruction);
void RiscVFltqD(const Instruction* instruction);

// Half precision Zfa instructions.
void RiscVFliH(const Instruction* instruction);
void RiscVFMinmH(const Instruction* instruction);
void RiscVFMaxmH(const Instruction* instruction);
void RiscVFRoundH(const Instruction* instruction);
void RiscVFRoundnxH(const Instruction* instruction);
void RiscVFleqH(const Instruction* instruction);
void RiscVFltqH(const Instruction* instruction);

// RV32-specific Zfa instructions.
void RiscVFmvhXD(const Instruction* instruction);
void RiscVFmvpDX(const Instruction* instruction);

}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_RISCV_RISCV_ZFA_INSTRUCTIONS_H_
