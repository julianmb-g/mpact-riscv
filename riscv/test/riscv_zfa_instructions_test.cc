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
  float a = -2.0f;
  float b = 3.0f;
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
  
  EXPECT_EQ(result, -2.0f);
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

}  // namespace test
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
