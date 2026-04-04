#include "riscv/riscv_dtb_loader.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include <filesystem>
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
  void CreateCrossCompiledElf(const std::string& path, uint64_t addr, uint64_t bss_size, const std::string& asm_text = "add a0, a0, a1\n  # Added authentic payload\n") {
    std::string tmp_dir = std::getenv("TEST_TMPDIR") ? std::getenv("TEST_TMPDIR") : ".os_build";
    std::filesystem::create_directories(tmp_dir);
    std::string asm_path = tmp_dir + "/temp_stub.s";
    
    struct FileCleaner {
      std::string p;
      ~FileCleaner() { std::remove(p.c_str()); }
    } cleaner{asm_path};

    std::ofstream s_file(asm_path);
    s_file << ".global _start\n_start:\n" << asm_text;
    if (bss_size > 0) {
      s_file << ".section .bss\n.space " << std::to_string(bss_size) << "\n";
    }
    s_file.close();
    std::string cmd = "riscv64-unknown-elf-gcc -nostdlib -T testfiles/vmlinux.ld " + asm_path + " -o " + path;
    int ret = system(cmd.c_str());
    if (ret != 0) {
      FAIL() << "Compiler not available, failing true E2E boot test.";
    }
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
    
    std::string tmp_dir = std::getenv("TEST_TMPDIR") ? std::getenv("TEST_TMPDIR") : ".os_build";
    std::filesystem::create_directories(tmp_dir);
    dtb_path_ = tmp_dir + "/dummy.dtb";

    std::ofstream dtb_file(dtb_path_, std::ios::binary);
    // 0xd00dfeed magic number + no extra bytes to strictly enforce boundary
    dtb_data_ = {0xd0, 0x0d, 0xfe, 0xed};
    dtb_file.write(reinterpret_cast<const char*>(dtb_data_.data()), dtb_data_.size());
    dtb_file.close();

    vmlinux_path_ = tmp_dir + "/dummy_vmlinux.elf";
    CreateCrossCompiledElf(vmlinux_path_, 0x20000000, 0x10000);

    conflict_path_ = tmp_dir + "/conflict_vmlinux.elf";
    CreateCrossCompiledElf(conflict_path_, 0x20000000, 0x300010);

    touching_path_ = tmp_dir + "/touching_vmlinux.elf";
    CreateCrossCompiledElf(touching_path_, 0x20000000, 0x2FFFF8);
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
  memory_->Load(0x20300000, db_dtb, nullptr, nullptr);
  for (size_t i = 0; i < dtb_data_.size(); ++i) {
    EXPECT_EQ(db_dtb->Get<uint8_t>(i), dtb_data_[i]);
  }
  db_dtb->DecRef();

  EXPECT_EQ(a0->data_buffer()->Get<uint64_t>(0), 0); // hartid
  EXPECT_EQ(a1->data_buffer()->Get<uint64_t>(0), 0x20300000); // kDtbAddress
}

TEST_F(RiscvDtbLoaderTest, MissingArtifactFailsOrganically) {
  std::string tmp_dir = std::getenv("TEST_TMPDIR") ? std::getenv("TEST_TMPDIR") : ".os_build";
  std::string missing_vmlinux = tmp_dir + "/non_existent_vmlinux.elf";
  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, missing_vmlinux, dtb_path_);
  
  EXPECT_TRUE(absl::IsNotFound(status));
  EXPECT_EQ(status.message(), absl::StrCat("Unable to open elf file: '", missing_vmlinux, "'"));
}

TEST_F(RiscvDtbLoaderTest, InvalidFdtMagicFails) {
  std::string tmp_dir = std::getenv("TEST_TMPDIR") ? std::getenv("TEST_TMPDIR") : ".os_build";
  std::filesystem::create_directories(tmp_dir);
  std::string bad_dtb_path = tmp_dir + "/bad_magic.dtb";
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
  std::string tmp_dir = std::getenv("TEST_TMPDIR") ? std::getenv("TEST_TMPDIR") : ".os_build";
  std::filesystem::create_directories(tmp_dir);
  // We need to compile an ELF containing the handshake test instructions.
  std::string entry_path = tmp_dir + "/handshake_vmlinux.elf";
  uint64_t entry_point = 0x20000000;
  // The test expects these exact instructions at the entry point.
  CreateCrossCompiledElf(entry_path, entry_point, 0, 
    "sw a0, 0(x0)\n"
    "sw a1, 4(x0)\n"
    "add a0, a0, a1\n  # Added authentic payload\n"
  );

  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, entry_path, dtb_path_);
  ASSERT_TRUE(status.ok()) << status.message();

  uint64_t expected_hartid = 0; // default state
  uint64_t expected_dtb_ptr = 0x20300000;

  EXPECT_TRUE(top_->WriteRegister("pc", entry_point).ok());

  // Step 2 instructions natively
  EXPECT_TRUE(top_->Step(2).ok());

  auto mem_db = state_->db_factory()->Allocate<uint32_t>(2);
  memory_->Load(0x0, mem_db, nullptr, nullptr);
  
  EXPECT_EQ(mem_db->Get<uint32_t>(0), expected_hartid) << "Organic execution: a0 (hartid) must be stored at 0x0";
  EXPECT_EQ(mem_db->Get<uint32_t>(1), expected_dtb_ptr) << "Organic execution: a1 (dtb pointer) must be stored at 0x4";
  
  mem_db->DecRef();
  std::remove(entry_path.c_str());
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
