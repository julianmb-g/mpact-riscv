#include "riscv/riscv_dtb_loader.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_register.h"
#include "riscv/riscv_top.h"
#include "riscv/riscv64_decoder.h"
#include "riscv/riscv_register_aliases.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/generic/data_buffer.h"
#include "utils/assembler/native_assembler_wrapper.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace {

using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::assembler::NativeTextualAssembler;

class RiscvDtbLoaderTest : public ::testing::Test {
 protected:
  void CreateMinimalElf(const std::string& path, uint64_t addr, uint64_t mem_size) {
    // Minimal 64-bit ELF with 1 PT_LOAD segment
    std::vector<uint8_t> elf(120, 0);
    // e_ident
    elf[0] = 0x7F; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
    elf[4] = 2; // ELFCLASS64
    elf[5] = 1; // ELFDATA2LSB
    elf[6] = 1; // EV_CURRENT
    // e_type = ET_EXEC (2)
    elf[16] = 2;
    // e_machine = EM_RISCV (243)
    elf[18] = 243;
    // e_version = 1
    elf[20] = 1;
    // e_entry = addr
    *reinterpret_cast<uint64_t*>(&elf[24]) = addr;
    // e_phoff = 64
    *reinterpret_cast<uint64_t*>(&elf[32]) = 64;
    // e_ehsize = 64
    *reinterpret_cast<uint16_t*>(&elf[52]) = 64;
    // e_phentsize = 56
    *reinterpret_cast<uint16_t*>(&elf[54]) = 56;
    // e_phnum = 1
    *reinterpret_cast<uint16_t*>(&elf[56]) = 1;

    // Program Header (starts at 64)
    // p_type = PT_LOAD (1)
    *reinterpret_cast<uint32_t*>(&elf[64]) = 1;
    // p_flags = PF_X | PF_R (5)
    *reinterpret_cast<uint32_t*>(&elf[68]) = 5;
    // p_offset = 120 (right after headers)
    *reinterpret_cast<uint64_t*>(&elf[72]) = 120;
    // p_vaddr = addr
    *reinterpret_cast<uint64_t*>(&elf[80]) = addr;
    // p_paddr = addr
    *reinterpret_cast<uint64_t*>(&elf[88]) = addr;
    // p_filesz = 0
    *reinterpret_cast<uint64_t*>(&elf[96]) = 0;
    // p_memsz = mem_size
    *reinterpret_cast<uint64_t*>(&elf[104]) = mem_size;
    // p_align = 0x1000
    *reinterpret_cast<uint64_t*>(&elf[112]) = 0x1000;

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(elf.data()), elf.size());
    out.close();
  }

  void SetUp() override {
    memory_ = new FlatDemandMemory();
    state_ = new RiscVState("test_state", RiscVXlen::RV64, memory_);
    decoder_ = new RiscV64Decoder(state_, memory_);
    
    // Ensure registers exist
    for (int i = 0; i < 32; i++) {
      std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
      state_->AddRegister<RV64Register>(reg_name);
      state_->AddRegisterAlias<RV64Register>(
          reg_name, kXRegisterAliases[i]);
    }
    top_ = new RiscVTop("test_top", state_, decoder_);
    
    dtb_path_ = std::string(::testing::TempDir()) + "/dummy.dtb";

    std::ofstream dtb_file(dtb_path_, std::ios::binary);
    // 0xd00dfeed magic number + no extra bytes to strictly enforce boundary
    dtb_data_ = {0xd0, 0x0d, 0xfe, 0xed};
    dtb_file.write(reinterpret_cast<const char*>(dtb_data_.data()), dtb_data_.size());
    dtb_file.close();

    vmlinux_path_ = std::string(::testing::TempDir()) + "/dummy_vmlinux.elf";
    CreateMinimalElf(vmlinux_path_, 0x200000, 0x10000);

    conflict_path_ = std::string(::testing::TempDir()) + "/conflict_vmlinux.elf";
    // Conflict with 0x21000000
    CreateMinimalElf(conflict_path_, 0x200000, 0x2000000);

    touching_path_ = std::string(::testing::TempDir()) + "/touching_vmlinux.elf";
    // exactly touches 0x21000000 but doesn't intersect
    CreateMinimalElf(touching_path_, 0x200000, 0x1000000);
  }

  void TearDown() override {
    std::remove(dtb_path_.c_str());
    std::remove(vmlinux_path_.c_str());
    std::remove(conflict_path_.c_str());
    std::remove(touching_path_.c_str());
    delete top_;
    delete decoder_;
    delete state_;
    delete memory_;
  }

  void LoadPayload(uint64_t entry_point, const std::string& asm_text) {
    NativeTextualAssembler assembler;
    auto elf_bytes = assembler.Assemble(asm_text);
    EXPECT_TRUE(elf_bytes.ok()) << elf_bytes.status().message();
    if (!elf_bytes.ok()) return;
    auto* db = state_->db_factory()->Allocate<uint8_t>(elf_bytes.value().size());
    std::memcpy(db->raw_ptr(), elf_bytes.value().data(), elf_bytes.value().size());
    memory_->Store(entry_point, db);
    db->DecRef();
  }

  FlatDemandMemory* memory_;
  RiscVState* state_;
  RiscVTop* top_;
  RiscV64Decoder* decoder_;
  std::string vmlinux_path_;
  std::string dtb_path_;
  std::string conflict_path_;
  std::string touching_path_;
  std::vector<uint8_t> dtb_data_;
};

TEST_F(RiscvDtbLoaderTest, LoadsFirmwareAndSeedsRegisters) {
  auto* a0 = state_->GetRegister<RV64Register>("x10").first;
  auto* a1 = state_->GetRegister<RV64Register>("x11").first;

  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, vmlinux_path_, dtb_path_);
  EXPECT_TRUE(status.ok()) << status.message();

  auto db_dtb = state_->db_factory()->Allocate<uint8_t>(dtb_data_.size());
  memory_->Load(0x21000000, db_dtb, nullptr, nullptr);
  for (size_t i = 0; i < dtb_data_.size(); ++i) {
    EXPECT_EQ(db_dtb->Get<uint8_t>(i), dtb_data_[i]);
  }
  db_dtb->DecRef();

  EXPECT_EQ(a0->data_buffer()->Get<uint64_t>(0), 0); // hartid
  EXPECT_EQ(a1->data_buffer()->Get<uint64_t>(0), 0x21000000); // kDtbAddress
}

TEST_F(RiscvDtbLoaderTest, MissingArtifactFailsOrganically) {
  std::string missing_vmlinux = std::string(::testing::TempDir()) + "/non_existent_vmlinux.elf";
  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, missing_vmlinux, dtb_path_);
  
  EXPECT_TRUE(absl::IsNotFound(status));
  EXPECT_EQ(status.message(), absl::StrCat("Unable to open elf file: '", missing_vmlinux, "'"));
}

TEST_F(RiscvDtbLoaderTest, InvalidFdtMagicFails) {
  std::string bad_dtb_path = std::string(::testing::TempDir()) + "/bad_magic.dtb";
  std::ofstream dtb_file(bad_dtb_path, std::ios::binary);
  std::vector<uint8_t> bad_data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  dtb_file.write(reinterpret_cast<const char*>(bad_data.data()), bad_data.size());
  dtb_file.close();

  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, vmlinux_path_, bad_dtb_path);
  EXPECT_TRUE(absl::IsInvalidArgument(status));
  EXPECT_EQ(status.message(), "Invalid FDT magic number");
  std::remove(bad_dtb_path.c_str());
}

TEST_F(RiscvDtbLoaderTest, BoundaryIntersectionFails) {
  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, conflict_path_, dtb_path_);
  EXPECT_TRUE(absl::IsInvalidArgument(status));
  EXPECT_NE(status.message().find("intersects with ELF segment"), std::string::npos);
}

TEST_F(RiscvDtbLoaderTest, TouchingBoundaryDoesNotIntersect) {
  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, touching_path_, dtb_path_);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(RiscvDtbLoaderTest, AuthenticE2EExecutionVerifyHandshake) {
  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, vmlinux_path_, dtb_path_);
  EXPECT_TRUE(status.ok()) << status.message();

  uint64_t expected_hartid = 0; // default state
  uint64_t expected_dtb_ptr = 0x21000000;

  uint64_t entry_point = 0x200000;
  
  LoadPayload(entry_point, 
    "sw a0, 0(x0)\n"
    "sw a1, 4(x0)\n"
    "nop\n"
  );

  EXPECT_TRUE(top_->WriteRegister("pc", entry_point).ok());

  // Step 2 instructions natively
  EXPECT_TRUE(top_->Step(2).ok());

  auto mem_db = state_->db_factory()->Allocate<uint32_t>(2);
  memory_->Load(0x0, mem_db, nullptr, nullptr);
  
  EXPECT_EQ(mem_db->Get<uint32_t>(0), expected_hartid) << "Organic execution: a0 (hartid) must be stored at 0x0";
  EXPECT_EQ(mem_db->Get<uint32_t>(1), expected_dtb_ptr) << "Organic execution: a1 (dtb pointer) must be stored at 0x4";
  
  mem_db->DecRef();
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
