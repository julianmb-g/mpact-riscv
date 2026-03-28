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
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/generic/data_buffer.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace {

using ::mpact::sim::util::FlatDemandMemory;

class RiscvDtbLoaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_ = new FlatDemandMemory();
    state_ = new RiscVState("test_state", RiscVXlen::RV64, memory_);
    
    // Ensure registers exist
    state_->AddRegister<RV64Register>("x10");
    state_->AddRegister<RV64Register>("x11");
    
    dtb_path_ = std::string(::testing::TempDir()) + "/dummy.dtb";

    std::ofstream dtb_file(dtb_path_, std::ios::binary);
    // 0xd00dfeed magic number + no extra bytes to strictly enforce boundary
    dtb_data_ = {0xd0, 0x0d, 0xfe, 0xed};
    dtb_file.write(reinterpret_cast<const char*>(dtb_data_.data()), dtb_data_.size());
    dtb_file.close();

    // Create a dummy vmlinux.elf
    vmlinux_path_ = std::string(::testing::TempDir()) + "/dummy_vmlinux.elf";
    std::string s_path = std::string(::testing::TempDir()) + "/dummy.s";
    std::ofstream s_file(s_path);
    s_file << ".global _start\n_start:\n  wfi\n";
    s_file.close();
    std::string cmd = "riscv64-unknown-elf-gcc -Ttext 0x20000000 -nostdlib -Wl,--nmagic " + s_path + " -o " + vmlinux_path_;
    int ret = system(cmd.c_str());
    EXPECT_EQ(ret, 0);

    // Create a conflicting vmlinux.elf
    conflict_path_ = std::string(::testing::TempDir()) + "/conflict_vmlinux.elf";
    std::string ld_path = std::string(::testing::TempDir()) + "/conflict.ld";
    std::ofstream ld_file(ld_path);
    ld_file << "SECTIONS { . = 0x20000000; .text : { *(.text) } . = 0x21000000; .data : { *(.data) } }\n";
    ld_file.close();

    std::string c_path = std::string(::testing::TempDir()) + "/conflict.s";
    std::ofstream c_file(c_path);
    c_file << ".section .text\n.global _start\n_start:\n  wfi\n.section .data\n  .word 0x12345678\n";
    c_file.close();
    std::string cmd2 = "riscv64-unknown-elf-gcc -nostdlib -Wl,-T," + ld_path + " " + c_path + " -o " + conflict_path_;
    ret = system(cmd2.c_str());
    EXPECT_EQ(ret, 0);
  }

  void TearDown() override {
    std::remove(dtb_path_.c_str());
    delete state_;
    delete memory_;
  }

  FlatDemandMemory* memory_;
  RiscVState* state_;
  std::string vmlinux_path_;
  std::string dtb_path_;
  std::string conflict_path_;
  std::vector<uint8_t> dtb_data_;
};

TEST_F(RiscvDtbLoaderTest, LoadsFirmwareAndSeedsRegisters) {
  auto* a0 = state_->GetRegister<RV64Register>("x10").first;
  auto* a1 = state_->GetRegister<RV64Register>("x11").first;

  // Use dummy ELF
  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, vmlinux_path_, dtb_path_);
  EXPECT_TRUE(status.ok()) << status.message();

  auto db_dtb = state_->db_factory()->Allocate<uint8_t>(dtb_data_.size());
  memory_->Load(0x21000000, db_dtb, nullptr, nullptr);
  for (size_t i = 0; i < dtb_data_.size(); ++i) {
    EXPECT_EQ(db_dtb->Get<uint8_t>(i), dtb_data_[i]);
  }
  db_dtb->DecRef();

  // Verify that dummy_vmlinux.elf loaded into memory (Entry point is 0x20000000)
  auto db_vmlinux = state_->db_factory()->Allocate<uint32_t>(1);
  memory_->Load(0x20000000, db_vmlinux, nullptr, nullptr);
  uint32_t entry_instr = db_vmlinux->Get<uint32_t>(0);
  EXPECT_NE(entry_instr, 0) << "dummy_vmlinux.elf was not loaded into memory";
  db_vmlinux->DecRef();

  EXPECT_EQ(a0->data_buffer()->Get<uint64_t>(0), 0); // hartid
  EXPECT_EQ(a1->data_buffer()->Get<uint64_t>(0), 0x21000000); // kDtbAddress
}

TEST_F(RiscvDtbLoaderTest, MissingArtifactFailsOrganically) {
  std::string missing_vmlinux = std::string(::testing::TempDir()) + "/non_existent_vmlinux.elf";
  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, missing_vmlinux, dtb_path_);
  
  // Must organically fail with NotFoundError, DO NOT use GTEST_SKIP!
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

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
