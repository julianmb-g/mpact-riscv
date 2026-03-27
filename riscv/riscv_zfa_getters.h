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

#ifndef THIRD_PARTY_MPACT_RISCV_RISCV_ZFA_GETTERS_H_
#define THIRD_PARTY_MPACT_RISCV_RISCV_ZFA_GETTERS_H_

#include <cstdint>

#include "mpact/sim/generic/immediate_operand.h"
#include "mpact/sim/generic/operand_interface.h"
#include "riscv/riscv_encoding_common.h"
#include "riscv/riscv_getter_helpers.h"

namespace mpact::sim::riscv {

using ::mpact::sim::generic::ImmediateOperand;

template <typename Enum, typename Extractors, typename IntRegister,
          typename FpRegister>
void AddRiscVZfaSourceGetters(SourceOpGetterMap& getter_map,
                              RiscVEncodingCommon* common) {
  // Source operand getters for Zfa.
  Insert(getter_map, *Enum::kZfaRTypeZfaRmImm, [common]() {
    return new ImmediateOperand<uint8_t>(
        Extractors::ZfaRType::ExtractZfaRmImm(common->inst_word()));
  });
  Insert(getter_map, *Enum::kZfaRTypeZfaRs1Imm, [common]() {
    return new ImmediateOperand<uint8_t>(
        Extractors::ZfaRType::ExtractZfaRs1Imm(common->inst_word()));
  });
}

template <typename Enum, typename Extractors, typename IntRegister,
          typename FpRegister>
void AddRiscVZfaDestGetters(DestOpGetterMap& getter_map,
                            RiscVEncodingCommon* common) {
  // Destination operand getters for Zfa.
}

}  // namespace mpact::sim::riscv

#endif  // THIRD_PARTY_MPACT_RISCV_RISCV_ZFA_GETTERS_H_
