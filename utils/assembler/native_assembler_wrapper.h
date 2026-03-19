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

#ifndef MPACT_RISCV_UTILS_ASSEMBLER_NATIVE_ASSEMBLER_WRAPPER_H_
#define MPACT_RISCV_UTILS_ASSEMBLER_NATIVE_ASSEMBLER_WRAPPER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "mpact/sim/util/asm/simple_assembler.h"
#include "mpact/sim/util/asm/opcode_assembler_interface.h"
#include "riscv/riscv64g_bin_encoder_interface.h"
#include "riscv/riscv64g_encoder.h"

namespace mpact::sim::assembler {

class NativeTextualAssembler {
 public:
  NativeTextualAssembler();
  ~NativeTextualAssembler();
  
  absl::StatusOr<std::vector<uint8_t>> Assemble(const std::string& asm_text);
  absl::Status EncodeInstruction(const std::string& mnemonic, 
                                 const std::vector<std::string>& operands, 
                                 uint32_t* encoded_out);
 private:
  std::unique_ptr<::mpact::sim::riscv::isa64::RiscV64GBinEncoderInterface> bin_encoder_interface_;
  std::unique_ptr<::mpact::sim::riscv::isa64::Riscv64gSlotMatcher> slot_matcher_;
  std::unique_ptr<::mpact::sim::util::assembler::OpcodeAssemblerInterface> assembler_wrapper_;
  std::unique_ptr<::mpact::sim::util::assembler::SimpleAssembler> core_assembler_;
};

}  // namespace mpact::sim::assembler

#endif  // MPACT_RISCV_UTILS_ASSEMBLER_NATIVE_ASSEMBLER_WRAPPER_H_
