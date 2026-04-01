#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "absl/strings/str_cat.h"
#include "riscv/riscv_top.h"
#include "riscv/rva23u64_decoder_wrapper.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_fp_state.h"
#include "riscv/riscv_register.h"
#include "riscv/riscv_register_aliases.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/util/memory/atomic_memory.h"
#include "mpact/sim/util/program_loader/elf_program_loader.h"
#include <cmath>

namespace {

using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVXlen;
using ::mpact::sim::riscv::RiscVFPState;
using ::mpact::sim::riscv::Rva23u64DecoderWrapper;
using ::mpact::sim::riscv::RiscVTop;
using ::mpact::sim::riscv::RV64Register;
using ::mpact::sim::riscv::RVFpRegister;
using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::util::AtomicMemory;
using ::mpact::sim::util::ElfProgramLoader;
using ::mpact::sim::riscv::ExceptionCode;

class RiscVTrapIntegrationTest : public ::testing::Test {
 protected:
  RiscVTrapIntegrationTest() {
    memory_ = new FlatDemandMemory();
    atomic_memory_ = new AtomicMemory(memory_);
    state_ = new RiscVState("test_trap", RiscVXlen::RV64, memory_, atomic_memory_);
    fp_state_ = new RiscVFPState(state_->csr_set(), state_);
    state_->set_rv_fp(fp_state_);

    decoder_ = new Rva23u64DecoderWrapper(state_, memory_);

    for (int i = 0; i < 32; i++) {
      std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
      state_->AddRegister<RV64Register>(reg_name);
      EXPECT_TRUE(state_->AddRegisterAlias<RV64Register>(reg_name, mpact::sim::riscv::kXRegisterAliases[i]).ok());
      
      std::string fp_name = absl::StrCat(RiscVState::kFregPrefix, i);
      state_->AddRegister<RVFpRegister>(fp_name);
      EXPECT_TRUE(state_->AddRegisterAlias<RVFpRegister>(fp_name, mpact::sim::riscv::kFRegisterAliases[i]).ok());
    }
    state_->AddRegister<RV64Register>("mstatus");
    
    top_ = new RiscVTop("test_top", state_, decoder_);
  }

  ~RiscVTrapIntegrationTest() override {
    delete top_;
    delete decoder_;
    delete state_;
    delete atomic_memory_;
    delete memory_;
  }

  uint64_t LoadPayload(const std::string& elf_path) {
    ElfProgramLoader loader(memory_);
    auto result = loader.LoadProgram(elf_path);
    EXPECT_TRUE(result.ok()) << "Failed to load ELF: " << result.status().message();
    return result.value();
  }

  FlatDemandMemory* memory_;
  AtomicMemory* atomic_memory_;
  RiscVState* state_;
  RiscVFPState* fp_state_;
  Rva23u64DecoderWrapper* decoder_;
  RiscVTop* top_;
};

TEST_F(RiscVTrapIntegrationTest, test_decoder_nullptr_yields_illegal_instruction) {
  uint64_t pc = LoadPayload("riscv/test/testfiles/trap_test.elf");
  
  auto status_reg = state_->GetRegister<RV64Register>("mstatus").first;
  status_reg->data_buffer()->Set<uint64_t>(0, 0x2000);

  auto status = top_->WriteRegister("pc", pc);
  ASSERT_TRUE(status.ok()) << status.message();
  
  auto step_status = top_->Step(1);
  
  // Natively verify that the unmapped opcode organically triggers a trap delegation 
  // via the authentic riscv_top.cc integration boundary.
  EXPECT_EQ(state_->mcause()->AsUint64(), static_cast<uint64_t>(ExceptionCode::kIllegalInstruction))
      << "Decoder failed to trap unmapped opcode into an architectural delegation (mcause not updated).";
  EXPECT_EQ(state_->mepc()->AsUint64(), pc)
      << "mepc was not properly updated to the trapped instruction's PC.";
}

} // namespace
