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

#ifndef MPACT_RISCV_RISCV_RISCV_ZICBO_INSTRUCTIONS_H_
#define MPACT_RISCV_RISCV_RISCV_ZICBO_INSTRUCTIONS_H_

#include "mpact/sim/generic/instruction.h"

namespace mpact::sim::riscv {

void RiscVCboInval(::mpact::sim::generic::Instruction* inst);
void RiscVCboClean(::mpact::sim::generic::Instruction* inst);
void RiscVCboFlush(::mpact::sim::generic::Instruction* inst);
void RiscVCboZero(::mpact::sim::generic::Instruction* inst);

}  // namespace mpact::sim::riscv

#endif  // MPACT_RISCV_RISCV_RISCV_ZICBO_INSTRUCTIONS_H_
