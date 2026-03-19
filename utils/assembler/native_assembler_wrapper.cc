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

#include "utils/assembler/native_assembler_wrapper.h"

#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "elfio/elf_types.hpp"
#include "elfio/elfio.hpp"
#include "mpact/sim/util/asm/opcode_assembler_interface.h"
#include "mpact/sim/util/asm/resolver_interface.h"
#include "mpact/sim/util/asm/simple_assembler.h"
#include "re2/re2.h"

namespace mpact::sim::assembler {

using ::mpact::sim::util::assembler::OpcodeAssemblerInterface;
using ::mpact::sim::util::assembler::RelocationInfo;
using ::mpact::sim::util::assembler::ResolverInterface;
using AddSymbolCallback =
    ::mpact::sim::util::assembler::OpcodeAssemblerInterface::AddSymbolCallback;

class NativeAssemblerWrapperImpl : public OpcodeAssemblerInterface {
 public:
  NativeAssemblerWrapperImpl(::mpact::sim::riscv::isa64::Riscv64gSlotMatcher* matcher)
      : label_re_("^(\\S+)\\s*:"), matcher_(matcher) {}
  ~NativeAssemblerWrapperImpl() override = default;

  absl::StatusOr<size_t> Encode(
      uint64_t address, absl::string_view text,
      AddSymbolCallback add_symbol_callback, ResolverInterface* resolver,
      std::vector<uint8_t>& bytes,
      std::vector<RelocationInfo>& relocations) override {
    std::string label;
    if (RE2::Consume(&text, label_re_, &label)) {
      auto status =
          add_symbol_callback(label, address, 0, ELFIO::STT_NOTYPE, 0, 0);
      if (!status.ok()) return status;
    }
    auto res = matcher_->Encode(address, text, 0, resolver, relocations);
    if (!res.status().ok()) return res.status();
    auto [value, size] = res.value();
    union {
      uint64_t i;
      uint8_t b[sizeof(uint64_t)];
    } u;
    u.i = value;
    for (int i = 0; i < size / 8; ++i) {
      bytes.push_back(u.b[i]);
    }
    return size / 8;
  }

 private:
  RE2 label_re_;
  ::mpact::sim::riscv::isa64::Riscv64gSlotMatcher* matcher_;
};

NativeTextualAssembler::NativeTextualAssembler() {
  bin_encoder_interface_ = std::make_unique<::mpact::sim::riscv::isa64::RiscV64GBinEncoderInterface>();
  slot_matcher_ = std::make_unique<::mpact::sim::riscv::isa64::Riscv64gSlotMatcher>(bin_encoder_interface_.get());
  auto status = slot_matcher_->Initialize();
  CHECK_OK(status);
  assembler_wrapper_ = std::make_unique<NativeAssemblerWrapperImpl>(slot_matcher_.get());
  core_assembler_ = std::make_unique<::mpact::sim::util::assembler::SimpleAssembler>("#", ELFIO::ELFCLASS64, assembler_wrapper_.get());
  core_assembler_->writer().set_os_abi(ELFIO::ELFOSABI_LINUX);
  core_assembler_->writer().set_machine(ELFIO::EM_RISCV);
}

NativeTextualAssembler::~NativeTextualAssembler() = default;

absl::StatusOr<std::vector<uint8_t>> NativeTextualAssembler::Assemble(const std::string& asm_text) {
  std::istringstream is(".text\n" + asm_text);
  auto parse_status = core_assembler_->Parse(is);
  if (!parse_status.ok()) {
    return absl::InvalidArgumentError("Parse failed: " + std::string(parse_status.message()));
  }
  auto reloc_status = core_assembler_->CreateRelocatable();
  if (!reloc_status.ok()) {
    return reloc_status;
  }
  
  ELFIO::section* text_section = core_assembler_->writer().sections[".text"];
  if (!text_section) {
    return absl::InternalError("No .text section generated");
  }
  if (text_section->get_size() < 4) {
    return absl::InternalError("Generated code too short");
  }

  const char* data = text_section->get_data();
  std::vector<uint8_t> result(data, data + text_section->get_size());
  return result;
}

absl::Status NativeTextualAssembler::EncodeInstruction(const std::string& mnemonic, 
                                                       const std::vector<std::string>& operands, 
                                                       uint32_t* encoded_out) {
  std::string asm_text = mnemonic;
  if (!operands.empty()) {
    asm_text += " ";
    for (size_t i = 0; i < operands.size(); ++i) {
      asm_text += operands[i];
      if (i < operands.size() - 1) {
        asm_text += ", ";
      }
    }
  }
  
  auto res = Assemble(asm_text);
  if (!res.ok()) {
    return res.status();
  }
  if (res.value().size() < 4) {
    return absl::InternalError("Payload too short");
  }
  
  std::memcpy(encoded_out, res.value().data(), sizeof(uint32_t));
  return absl::OkStatus();
}

}  // namespace mpact::sim::assembler
