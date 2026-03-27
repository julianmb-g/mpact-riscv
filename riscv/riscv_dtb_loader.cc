#include "riscv/riscv_dtb_loader.h"
#include <fstream>
#include <vector>
#include <filesystem>
#include "absl/strings/str_cat.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/util/program_loader/elf_program_loader.h"

namespace mpact {
namespace sim {
namespace riscv {

constexpr uint64_t kDtbAddress = 0x203F0000;

absl::Status RiscvDtbLoader::LoadFirmwareAndSeedRegisters(
    RiscVState* state, 
    const std::string& vmlinux_payload_path, 
    const std::string& dtb_payload_path) {
  
  if (state == nullptr || state->memory() == nullptr) {
    return absl::InvalidArgumentError("Invalid state or memory pointer");
  }

  // Load vmlinux via ELF Loader
  mpact::sim::util::ElfProgramLoader elf_loader(state->memory());
  auto load_result = elf_loader.LoadProgram(vmlinux_payload_path);
  if (!load_result.ok()) {
    if (absl::IsNotFound(load_result.status())) {
      return load_result.status();
    }
    // Evasion ban: if it's there but parsing fails or something, or if it doesn't exist, we must return NotFound for missing file, 
    // but elf_loader might return a different error. Let's explicitly check file existence to return NotFoundError if missing.
    if (!std::filesystem::exists(vmlinux_payload_path)) {
      return absl::NotFoundError(absl::StrCat("Unable to open elf file: '", vmlinux_payload_path, "'"));
    }
    return load_result.status();
  }

  // Load dtb
  std::ifstream dtb_file(dtb_payload_path, std::ios::binary | std::ios::ate);
  if (!dtb_file) {
    return absl::NotFoundError(absl::StrCat("Unable to open dtb file: '", dtb_payload_path, "'"));
  }
  std::streamsize dtb_size = dtb_file.tellg();
  dtb_file.seekg(0, std::ios::beg);
  std::vector<uint8_t> dtb_data(dtb_size);
  if (dtb_size > 0 && !dtb_file.read(reinterpret_cast<char*>(dtb_data.data()), dtb_size)) {
      return absl::InternalError("Failed to read dtb payload");
  }

  auto db_factory = state->db_factory();

  // Map dtb into enforced safe zone
  if (dtb_size > 0) {
    auto db_dtb = db_factory->Allocate<uint8_t>(dtb_size);
    std::memcpy(db_dtb->Get<uint8_t>().data(), dtb_data.data(), dtb_size);
    state->memory()->Store(kDtbAddress, db_dtb);
    db_dtb->DecRef();
  }

  // Seed registers: a0 = hartid, a1 = dtb_address
  uint64_t hartid = 0; // Seeding hartid 0 for core 0
  
  auto a0_it = state->registers()->find("x10");
  if (a0_it != state->registers()->end()) {
    auto db_a0 = db_factory->Allocate<uint64_t>(1);
    db_a0->Set<uint64_t>(0, hartid);
    a0_it->second->SetDataBuffer(db_a0);
    db_a0->DecRef();
  }

  auto a1_it = state->registers()->find("x11");
  if (a1_it != state->registers()->end()) {
    auto db_a1 = db_factory->Allocate<uint64_t>(1);
    db_a1->Set<uint64_t>(0, kDtbAddress);
    a1_it->second->SetDataBuffer(db_a1);
    db_a1->DecRef();
  }

  return absl::OkStatus();
}

}  // namespace riscv
}  // namespace sim
}  // namespace mpact
