#include "riscv/riscv_zfa_instructions.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "riscv/riscv_fp_info.h"
#include "riscv/riscv_register.h"
#include "riscv/riscv_state.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace test {

using ::testing::Eq;

class RiscVZfaInstructionsTest : public ::testing::Test {
 protected:
  RiscVZfaInstructionsTest() {
    state_ = new RiscVState("test_state", RiscVXlen::RV64, nullptr, nullptr);
    instruction_ = new generic::Instruction(0, state_);
    instruction_->set_size(4);
    
    // Create destination register
    dest_reg_ = state_->GetRegister<RVFpRegister>("f1").first;
    auto dest_op = new generic::RegisterDestinationOperand<RVFpRegister::ValueType>(
        dest_reg_, 0);
    instruction_->AppendDestination(dest_op);
    
    // Flag reg
    flag_reg_ = state_->GetRegister<RV32Register>("fflags").first;
    auto flag_op = new generic::RegisterDestinationOperand<RV32Register::ValueType>(
        flag_reg_, 0);
    instruction_->AppendDestination(flag_op);
  }

  ~RiscVZfaInstructionsTest() override {
    instruction_->DecRef();
    delete state_;
  }

  RiscVState* state_;
  generic::Instruction* instruction_;
  RVFpRegister* dest_reg_;
  RV32Register* flag_reg_;
};

TEST_F(RiscVZfaInstructionsTest, FliS_Constant16) {
  auto src_op = new generic::ImmediateOperand<uint32_t>(16, "rs1");
  instruction_->AppendSource(src_op);
  
  RiscVFliS(instruction_);
  
  float result;
  auto val = dest_reg_->data_buffer()->Get<RVFpRegister::ValueType>(0);
  uint32_t uval = static_cast<uint32_t>(val);
  std::memcpy(&result, &uval, sizeof(float));
  
  EXPECT_EQ(result, 1.0f);
}

TEST_F(RiscVZfaInstructionsTest, FminmS_Magnitudes) {
  float a = -3.0f;
  float b = 2.0f;
  uint32_t a_bits;
  std::memcpy(&a_bits, &a, sizeof(float));
  uint32_t b_bits;
  std::memcpy(&b_bits, &b, sizeof(float));
  uint64_t nan_boxed_a = 0xffffffff00000000ULL | a_bits;
  uint64_t nan_boxed_b = 0xffffffff00000000ULL | b_bits;
  
  auto src_op_a = new generic::ImmediateOperand<uint64_t>(nan_boxed_a, "rs1");
  auto src_op_b = new generic::ImmediateOperand<uint64_t>(nan_boxed_b, "rs2");
  instruction_->AppendSource(src_op_a);
  instruction_->AppendSource(src_op_b);
  
  RiscVFMinmS(instruction_);
  
  float result;
  auto val = dest_reg_->data_buffer()->Get<RVFpRegister::ValueType>(0);
  uint32_t uval = static_cast<uint32_t>(val);
  std::memcpy(&result, &uval, sizeof(float));
  
  EXPECT_EQ(result, 2.0f);
}

TEST_F(RiscVZfaInstructionsTest, FminmS_NaNs) {
  float a = std::numeric_limits<float>::quiet_NaN();
  float b = std::numeric_limits<float>::quiet_NaN();
  uint32_t a_bits;
  std::memcpy(&a_bits, &a, sizeof(float));
  uint32_t b_bits;
  std::memcpy(&b_bits, &b, sizeof(float));
  uint64_t nan_boxed_a = 0xffffffff00000000ULL | a_bits;
  uint64_t nan_boxed_b = 0xffffffff00000000ULL | b_bits;
  
  auto src_op_a = new generic::ImmediateOperand<uint64_t>(nan_boxed_a, "rs1");
  auto src_op_b = new generic::ImmediateOperand<uint64_t>(nan_boxed_b, "rs2");
  instruction_->AppendSource(src_op_a);
  instruction_->AppendSource(src_op_b);
  
  RiscVFMinmS(instruction_);
  
  auto val = dest_reg_->data_buffer()->Get<RVFpRegister::ValueType>(0);
  uint32_t uval = static_cast<uint32_t>(val);
  
  // Check canonical NaN (0x7fc00000)
  EXPECT_EQ(uval, 0x7fc00000);
}



TEST_F(RiscVZfaInstructionsTest, TestZfaFminmFmaxmSemantics) {
  // Test FminmS with equal magnitudes
  float a = -2.0f;
  float b = 2.0f;
  uint32_t a_bits, b_bits;
  std::memcpy(&a_bits, &a, sizeof(float));
  std::memcpy(&b_bits, &b, sizeof(float));
  uint64_t nan_boxed_a = 0xffffffff00000000ULL | a_bits;
  uint64_t nan_boxed_b = 0xffffffff00000000ULL | b_bits;
  
  auto src_op_a = new generic::ImmediateOperand<uint64_t>(nan_boxed_a, "rs1");
  auto src_op_b = new generic::ImmediateOperand<uint64_t>(nan_boxed_b, "rs2");
  instruction_->AppendSource(src_op_a);
  instruction_->AppendSource(src_op_b);
  
  RiscVFMinmS(instruction_);
  
  float result;
  auto val = dest_reg_->data_buffer()->Get<RVFpRegister::ValueType>(0);
  uint32_t uval = static_cast<uint32_t>(val);
  std::memcpy(&result, &uval, sizeof(float));
  EXPECT_EQ(result, -2.0f); // Minimum magnitude with same absolute value is the negative one
  
  // Create a new instruction for FMaxmS to avoid reusing the state
  auto instruction2_ = new generic::Instruction(0, state_);
  instruction2_->set_size(4);
  auto dest_op2 = new generic::RegisterDestinationOperand<RVFpRegister::ValueType>(dest_reg_, 0);
  instruction2_->AppendDestination(dest_op2);
  
  auto fflags_op2 = new generic::RegisterDestinationOperand<RV32Register::ValueType>(flag_reg_, 0);
  instruction2_->AppendDestination(fflags_op2);
  
  auto src_op_a2 = new generic::ImmediateOperand<uint64_t>(nan_boxed_a, "rs1");
  auto src_op_b2 = new generic::ImmediateOperand<uint64_t>(nan_boxed_b, "rs2");
  instruction2_->AppendSource(src_op_a2);
  instruction2_->AppendSource(src_op_b2);
  
  RiscVFMaxmS(instruction2_);
  
  val = dest_reg_->data_buffer()->Get<RVFpRegister::ValueType>(0);
  uval = static_cast<uint32_t>(val);
  std::memcpy(&result, &uval, sizeof(float));
  EXPECT_EQ(result, 2.0f); // Maximum magnitude is the positive one
  
  instruction2_->DecRef();
}



TEST_F(RiscVZfaInstructionsTest, TestFmvhXD) {
  auto inst2 = new generic::Instruction(0, state_);
  inst2->set_size(4);
  
  uint64_t frs1_val = 0xAABBCCDD11223344ULL;
  auto src_op = new generic::ImmediateOperand<uint64_t>(frs1_val, "frs1");
  inst2->AppendSource(src_op);
  
  auto rd_reg = state_->GetRegister<RV32Register>("x1").first;
  auto rd_op = new generic::RegisterDestinationOperand<RV32Register::ValueType>(rd_reg, 0);
  inst2->AppendDestination(rd_op);

  RiscVFmvhXD(inst2);

  auto val = rd_reg->data_buffer()->Get<uint32_t>(0);
  EXPECT_EQ(val, 0xAABBCCDD);
  inst2->DecRef();
}

TEST_F(RiscVZfaInstructionsTest, TestFmvpDX) {
  auto inst2 = new generic::Instruction(0, state_);
  inst2->set_size(4);
  
  uint32_t rs1_val = 0x11223344;
  uint32_t rs2_val = 0xAABBCCDD;
  
  auto src_op1 = new generic::ImmediateOperand<uint32_t>(rs1_val, "rs1");
  auto src_op2 = new generic::ImmediateOperand<uint32_t>(rs2_val, "rs2");
  inst2->AppendSource(src_op1);
  inst2->AppendSource(src_op2);
  
  auto dest_op = new generic::RegisterDestinationOperand<RVFpRegister::ValueType>(dest_reg_, 0);
  inst2->AppendDestination(dest_op);
  
  RiscVFmvpDX(inst2);

  auto val = dest_reg_->data_buffer()->Get<uint64_t>(0);
  EXPECT_EQ(val, 0xAABBCCDD11223344ULL);
  inst2->DecRef();
}


}  // namespace test
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
