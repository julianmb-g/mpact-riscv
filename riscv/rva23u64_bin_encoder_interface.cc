// Copyright 2025 Google LLC
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

#include "riscv/rva23u64_bin_encoder_interface.h"

#include <cstdint>
#include <tuple>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/util/asm/resolver_interface.h"
#include "riscv/rva23u64_bin_encoder.h"
#include "riscv/rva23u64_encoder.h"
#include "riscv/rva23u64_enums.h"
#include "riscv/riscv_bin_setters.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace isa_rva23u64 {

using ::mpact::sim::generic::operator*;  // NOLINT(misc-unused-using-decls)
using ::mpact::sim::util::assembler::ResolverInterface;

Rva23u64BinEncoderInterface::Rva23u64BinEncoderInterface() {
  AddRiscvSourceOpBinSetters<SourceOpEnum, OpMap, isa_rva23u64::Encoder>(
      source_op_map_);
  AddRiscvDestOpBinSetters<DestOpEnum, OpMap, isa_rva23u64::Encoder>(
      dest_op_map_);
  AddRiscvSourceOpRelocationSetters<OpcodeEnum, SourceOpEnum, RelocationMap>(
      relocation_source_op_map_);

  source_op_map_[static_cast<int>(SourceOpEnum::kZfaRTypeZfaRmImm)] =
         [](uint64_t address, absl::string_view text,
            mpact::sim::util::assembler::ResolverInterface* resolver) -> absl::StatusOr<uint64_t> {
           static absl::flat_hash_map<absl::string_view, uint64_t> map = {
               {"rne", 0}, {"rtz", 1}, {"rdn", 2}, {"rup", 3}, {"rmm", 4}, {"dyn", 7}};
           auto iter = map.find(text);
           if (iter == map.end()) return absl::InvalidArgumentError("Invalid rm");
           return isa_rva23u64::Encoder::ZfaRType::InsertZfaRmImm(iter->second, 0ULL);
         };

  source_op_map_[static_cast<int>(SourceOpEnum::kZfaRTypeZfaRs1Imm)] =
         [](uint64_t address, absl::string_view text,
            mpact::sim::util::assembler::ResolverInterface* resolver) -> absl::StatusOr<uint64_t> {
           static absl::flat_hash_map<absl::string_view, uint64_t> map(kRegisterList);
           auto iter = map.find(text);
           if (iter == map.end()) return absl::InvalidArgumentError("Invalid src");
           return isa_rva23u64::Encoder::ZfaRType::InsertZfaRs1Imm(iter->second, 0ULL);
         };

  source_op_map_[static_cast<int>(SourceOpEnum::kUimm6)] =
         [](uint64_t address, absl::string_view text,
            mpact::sim::util::assembler::ResolverInterface* resolver) -> absl::StatusOr<uint64_t> {
           auto res = SimpleTextToInt<uint32_t>(text, resolver);
           if (!res.ok()) return res.status();
           return isa_rva23u64::Encoder::RSType::InsertRUimm6(res.value(), 0ULL);
         };
}

absl::StatusOr<std::tuple<uint64_t, int>>
Rva23u64BinEncoderInterface::GetOpcodeEncoding(SlotEnum slot, int entry,
                                               OpcodeEnum opcode,
                                               ResolverInterface* resolver) {
  return isa_rva23u64::kOpcodeEncodings->at(opcode);
}

absl::StatusOr<uint64_t> Rva23u64BinEncoderInterface::GetSrcOpEncoding(
    uint64_t address, absl::string_view text, SlotEnum slot, int entry,
    OpcodeEnum opcode, SourceOpEnum source_op, int source_num,
    ResolverInterface* resolver) {
  auto iter = source_op_map_.find(*source_op);
  if (iter == source_op_map_.end()) {
    return absl::NotFoundError(absl::StrCat(
        "Source operand not found for op enum value ", *source_op));
  }
  return iter->second(address, text, resolver);
}

absl::Status Rva23u64BinEncoderInterface::AppendSrcOpRelocation(
    uint64_t address, absl::string_view text, SlotEnum slot, int entry,
    OpcodeEnum opcode, SourceOpEnum source_op, int source_num,
    ResolverInterface* resolver, std::vector<RelocationInfo>& relocations) {
  auto iter = relocation_source_op_map_.find(std::tie(opcode, source_op));
  if (iter == relocation_source_op_map_.end()) return absl::OkStatus();
  return iter->second(address, text, resolver, relocations);
}

absl::StatusOr<uint64_t> Rva23u64BinEncoderInterface::GetDestOpEncoding(
    uint64_t address, absl::string_view text, SlotEnum slot, int entry,
    OpcodeEnum opcode, DestOpEnum dest_op, int dest_num,
    ResolverInterface* resolver) {
  auto iter = dest_op_map_.find(*dest_op);
  if (iter == dest_op_map_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Dest operand not found for op enum value ", *dest_op));
  }
  return iter->second(address, text, resolver);
}

absl::StatusOr<uint64_t> Rva23u64BinEncoderInterface::GetListDestOpEncoding(
    uint64_t address, absl::string_view text, SlotEnum slot, int entry,
    OpcodeEnum opcode, ListDestOpEnum dest_op, int dest_num,
    ResolverInterface* resolver) {
  auto iter = list_dest_op_map_.find(*dest_op);
  if (iter == list_dest_op_map_.end()) {
    return absl::NotFoundError(absl::StrCat(
        "List dest operand not found for op enum value ", *dest_op));
  }
  return iter->second(address, text, resolver);
}

absl::Status Rva23u64BinEncoderInterface::AppendDestOpRelocation(
    uint64_t address, absl::string_view text, SlotEnum slot, int entry,
    OpcodeEnum opcode, DestOpEnum dest_op, int dest_num,
    ResolverInterface* resolver, std::vector<RelocationInfo>& relocations) {
  // There are no destination operands that require relocation.
  return absl::OkStatus();
}

absl::StatusOr<uint64_t> Rva23u64BinEncoderInterface::GetListSrcOpEncoding(
    uint64_t address, absl::string_view text, SlotEnum slot, int entry,
    OpcodeEnum opcode, ListSourceOpEnum source_op, int source_num,
    ResolverInterface* resolver) {
  auto iter = list_source_op_map_.find(*source_op);
  if (iter == list_source_op_map_.end()) {
    return absl::NotFoundError(absl::StrCat(
        "List source operand not found for op enum value ", *source_op));
  }
  return iter->second(address, text, resolver);
}

absl::StatusOr<uint64_t> Rva23u64BinEncoderInterface::GetPredOpEncoding(
    uint64_t address, absl::string_view text, SlotEnum slot, int entry,
    OpcodeEnum opcode, PredOpEnum pred_op, ResolverInterface* resolver) {
  auto iter = pred_op_map_.find(*pred_op);
  if (iter == pred_op_map_.end()) {
    return absl::NotFoundError(absl::StrCat(
        "Predicate operand not found for op enum value ", *pred_op));
  }
  return iter->second(address, text, resolver);
}

}  // namespace isa_rva23u64
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
