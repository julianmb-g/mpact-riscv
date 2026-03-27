#include "riscv/riscv_dtb_loader.h"
#include <fstream>
#include <vector>
#include "absl/strings/str_cat.h"
#include "mpact/sim/generic/data_buffer.h"

namespace mpact {
namespace sim {
namespace riscv {

constexpr uint64_t kVmlinuxAddress = 0x20000000;
constexpr uint64_t kDtbAddress = 0x203F0000;

absl::Status RiscvDtbLoader::LoadFirmwareAndSeedRegisters(
    RiscVState* state, 
    const std::string& vmlinux_payload_path, 
    const std::string& dtb_payload_path) {
  
  if (state == nullptr || state->memory() == nullptr) {
    return absl::InvalidArgumentError("Invalid state or memory pointer");
  }

  // Load vmlinux
  std::ifstream vmlinux_file(vmlinux_payload_path, std::ios::binary | std::ios::ate);
  if (!vmlinux_file) {
    return absl::NotFoundError(absl::StrCat("Unable to open elf file: '", vmlinux_payload_path, "'"));
  }
  std::streamsize vmlinux_size = vmlinux_file.tellg();
  vmlinux_file.seekg(0, std::ios::beg);
  std::vector<uint8_t> vmlinux_data(vmlinux_size);
  if (vmlinux_size > 0 && !vmlinux_file.read(reinterpret_cast<char*>(vmlinux_data.data()), vmlinux_size)) {
      return absl::InternalError("Failed to read vmlinux payload");
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

  // Map vmlinux into EXTMEM
  if (vmlinux_size > 0) {
    auto db_vmlinux = db_factory->Allocate<uint8_t>(vmlinux_size);
    std::memcpy(db_vmlinux->Get<uint8_t>().data(), vmlinux_data.data(), vmlinux_size);
    state->memory()->Store(kVmlinuxAddress, db_vmlinux);
    db_vmlinux->DecRef();
  }

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
