#include "riscv/riscv_zicfiss_instructions.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "riscv/riscv_csr.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_register.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "riscv/riscv_instruction_helpers.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace {

using ::mpact::sim::generic::Instruction;

class RiscVZicfissInstructionsTest : public testing::Test {
 protected:
  RiscVZicfissInstructionsTest() {
    memory_ = new util::FlatDemandMemory(0);
    state_ = new RiscVState("test", RiscVXlen::RV64, memory_);
    
    auto res = state_->csr_set()->GetCsr(static_cast<uint64_t>(RiscVCsrEnum::kSsp));
    EXPECT_TRUE(res.ok());
    ssp_csr_ = static_cast<RiscVSimpleCsr<uint64_t>*>(res.value());
    ssp_csr_->Set(static_cast<uint64_t>(0x1000));
    
    inst_ = new Instruction(0, state_);
  }

  ~RiscVZicfissInstructionsTest() override {
    delete inst_;
    delete state_;
    delete memory_;
  }

  util::FlatDemandMemory *memory_;
  RiscVState *state_;
  RiscVSimpleCsr<uint64_t> *ssp_csr_;
  Instruction *inst_;
};

TEST_F(RiscVZicfissInstructionsTest, Sspush) {
  auto *src_op = new generic::ImmediateOperand<uint64_t>(0x12345678, "imm");
  inst_->AppendSource(src_op);

  RiscVSspush(inst_);

  EXPECT_EQ(ssp_csr_->GetUint64(), 0x1000 - 8);

  auto *load_db = state_->db_factory()->Allocate<uint64_t>(1);
  state_->LoadMemory(inst_, 0x1000 - 8, load_db, nullptr, nullptr);
  EXPECT_EQ(load_db->Get<uint64_t>(0), 0x12345678);
  load_db->DecRef();
}

TEST_F(RiscVZicfissInstructionsTest, Ssrdp) {
  auto rd_reg = state_->GetRegister<RV64Register>("x1").first;
  auto rd_op = new generic::RegisterDestinationOperand<RV64Register::ValueType>(rd_reg, 0);
  inst_->AppendDestination(rd_op);
  
  RiscVSsrdp(inst_);

  EXPECT_EQ(rd_reg->data_buffer()->Get<uint64_t>(0), 0x1000);
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
