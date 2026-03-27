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
    dtb_data_ = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    dtb_file.write(reinterpret_cast<const char*>(dtb_data_.data()), dtb_data_.size());
    dtb_file.close();
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
  std::vector<uint8_t> dtb_data_;
};

TEST_F(RiscvDtbLoaderTest, LoadsFirmwareAndSeedsRegisters) {
  auto* a0 = state_->GetRegister<RV64Register>("x10").first;
  auto* a1 = state_->GetRegister<RV64Register>("x11").first;

  // Use testfiles/hello_world.elf as a real ELF artifact
  vmlinux_path_ = "riscv/test/testfiles/hello_world.elf";

  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, vmlinux_path_, dtb_path_);
  EXPECT_TRUE(status.ok()) << status.message();

  auto db_dtb = state_->db_factory()->Allocate<uint8_t>(dtb_data_.size());
  memory_->Load(0x203F0000, db_dtb, nullptr, nullptr);
  for (size_t i = 0; i < dtb_data_.size(); ++i) {
    EXPECT_EQ(db_dtb->Get<uint8_t>(i), dtb_data_[i]);
  }
  db_dtb->DecRef();

  // Verify that hello_world.elf loaded into memory (Entry point is 0x80000000)
  auto db_vmlinux = state_->db_factory()->Allocate<uint32_t>(1);
  memory_->Load(0x80000000, db_vmlinux, nullptr, nullptr);
  uint32_t entry_instr = db_vmlinux->Get<uint32_t>(0);
  EXPECT_NE(entry_instr, 0) << "hello_world.elf was not loaded into EXTMEM";
  db_vmlinux->DecRef();

  EXPECT_EQ(a0->data_buffer()->Get<uint64_t>(0), 0); // hartid
  EXPECT_EQ(a1->data_buffer()->Get<uint64_t>(0), 0x203F0000); // kDtbAddress
}

TEST_F(RiscvDtbLoaderTest, MissingArtifactFailsOrganically) {
  std::string missing_vmlinux = std::string(::testing::TempDir()) + "/non_existent_vmlinux.elf";
  absl::Status status = RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state_, missing_vmlinux, dtb_path_);
  
  // Must organically fail with NotFoundError, DO NOT use GTEST_SKIP!
  EXPECT_TRUE(absl::IsNotFound(status));
  EXPECT_EQ(status.message(), absl::StrCat("Unable to open elf file: '", missing_vmlinux, "'"));
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
