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
#include "utils/assembler/native_assembler_wrapper.h"
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
using ::mpact::sim::assembler::NativeTextualAssembler;

class RiscVZfaIntegrationTest : public ::testing::Test {
 protected:
  RiscVZfaIntegrationTest() {
    memory_ = new FlatDemandMemory();
    atomic_memory_ = new AtomicMemory(memory_);
    state_ = new RiscVState("test_zfa", RiscVXlen::RV64, memory_, atomic_memory_);
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

  ~RiscVZfaIntegrationTest() override {
    delete top_;
    delete decoder_;
    delete state_;
    delete atomic_memory_;
    delete memory_;
  }

  void LoadPayload(uint64_t entry_point, const std::string& asm_text) {
    NativeTextualAssembler assembler;
    auto elf_bytes = assembler.Assemble(asm_text);
    ASSERT_TRUE(elf_bytes.ok()) << elf_bytes.status().message();
    auto* db = state_->db_factory()->Allocate<uint8_t>(elf_bytes.value().size());
    std::memcpy(db->raw_ptr(), elf_bytes.value().data(), elf_bytes.value().size());
    memory_->Store(entry_point, db);
    db->DecRef();
  }

  void RunProgram(uint64_t start_pc) {
    auto status = top_->WriteRegister("pc", start_pc);
    ASSERT_TRUE(status.ok()) << status.message();
    auto step_status = top_->Step(100);
    // ebreak will trap and may return an aborted or internal status depending on the exception handler.
    // We only care that the instruction prior to ebreak executed organically.
    if (!step_status.ok()) {
      // Allow execution to halt gracefully on ebreak trap
    }
  }

  FlatDemandMemory* memory_;
  AtomicMemory* atomic_memory_;
  RiscVState* state_;
  RiscVFPState* fp_state_;
  Rva23u64DecoderWrapper* decoder_;
  RiscVTop* top_;
};

TEST_F(RiscVZfaIntegrationTest, FminmS_OrganicExecution) {
  std::string asm_text = 
      ".text\n"
      "fminm.s f2, f0, f1\n"
      "ebreak\n";

  uint64_t pc = 0x10000;
  LoadPayload(pc, asm_text);
  
  auto status_reg = state_->GetRegister<RV64Register>("mstatus").first;
  status_reg->data_buffer()->Set<uint64_t>(0, 0x2000);

  uint32_t val_f0 = 0xc0400000; // -3.0f
  uint64_t boxed_f0 = 0xffffffff00000000ULL | val_f0;
  ASSERT_TRUE(top_->WriteRegister("f0", boxed_f0).ok());

  uint32_t val_f1 = 0x40000000; // 2.0f
  uint64_t boxed_f1 = 0xffffffff00000000ULL | val_f1;
  ASSERT_TRUE(top_->WriteRegister("f1", boxed_f1).ok());

  RunProgram(pc);

  auto result_val = top_->ReadRegister("f2");
  ASSERT_TRUE(result_val.ok());
  EXPECT_EQ(result_val.value() & 0xFFFFFFFF, 0x40000000); // 2.0f
}

TEST_F(RiscVZfaIntegrationTest, FminmS_NaNs) {
  std::string asm_text = 
      ".text\n"
      "fminm.s f2, f0, f1\n"
      "ebreak\n";

  uint64_t pc = 0x10000;
  LoadPayload(pc, asm_text);
  
  auto status_reg = state_->GetRegister<RV64Register>("mstatus").first;
  status_reg->data_buffer()->Set<uint64_t>(0, 0x2000);

  float nan_f = std::numeric_limits<float>::quiet_NaN();
  uint32_t val_f0;
  std::memcpy(&val_f0, &nan_f, sizeof(float));
  uint64_t boxed_f0 = 0xffffffff00000000ULL | val_f0;
  ASSERT_TRUE(top_->WriteRegister("f0", boxed_f0).ok());

  uint32_t val_f1 = 0x40000000; // 2.0f
  uint64_t boxed_f1 = 0xffffffff00000000ULL | val_f1;
  ASSERT_TRUE(top_->WriteRegister("f1", boxed_f1).ok());

  RunProgram(pc);

  auto result_val = top_->ReadRegister("f2");
  ASSERT_TRUE(result_val.ok());
  EXPECT_EQ(result_val.value() & 0xFFFFFFFF, 0x40000000); // non-NaN wins
}

TEST_F(RiscVZfaIntegrationTest, FmaxmS_EqualMagnitude) {
  std::string asm_text = 
      ".text\n"
      "fmaxm.s f2, f0, f1\n"
      "ebreak\n";

  uint64_t pc = 0x10000;
  LoadPayload(pc, asm_text);
  
  auto status_reg = state_->GetRegister<RV64Register>("mstatus").first;
  status_reg->data_buffer()->Set<uint64_t>(0, 0x2000);

  uint32_t val_f0 = 0xc0000000; // -2.0f
  uint64_t boxed_f0 = 0xffffffff00000000ULL | val_f0;
  ASSERT_TRUE(top_->WriteRegister("f0", boxed_f0).ok());

  uint32_t val_f1 = 0x40000000; // 2.0f
  uint64_t boxed_f1 = 0xffffffff00000000ULL | val_f1;
  ASSERT_TRUE(top_->WriteRegister("f1", boxed_f1).ok());

  RunProgram(pc);

  auto result_val = top_->ReadRegister("f2");
  ASSERT_TRUE(result_val.ok());
  EXPECT_EQ(result_val.value() & 0xFFFFFFFF, 0x40000000); // positive wins
}

} // namespace
