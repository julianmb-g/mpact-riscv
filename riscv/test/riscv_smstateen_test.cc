// Copyright 2025 Google LLC
#include "gtest/gtest.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_sim_csrs.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace {

class RiscVSmstateenTest : public ::testing::Test {
 protected:
  void SetUp() override {
    state_ = new RiscVState("test_state", RiscVXlen::RV64, nullptr);
  }
  
  void TearDown() override {
    delete state_;
  }

  RiscVState* state_;
};

TEST_F(RiscVSmstateenTest, MStateEn0Exists) {
  MStateEn0Csr csr(state_);
  EXPECT_EQ(csr.name(), "mstateen0");
  
  csr.Write(static_cast<uint64_t>(0x5555AAAA5555AAAAULL));
  EXPECT_EQ(csr.GetUint64(), 0x5555AAAA5555AAAAULL);
}

TEST_F(RiscVSmstateenTest, SStateEn0Exists) {
  SStateEn0Csr csr(state_);
  EXPECT_EQ(csr.name(), "sstateen0");
  
  csr.Write(static_cast<uint64_t>(0x12345678ULL));
  EXPECT_EQ(csr.GetUint64(), 0x12345678ULL);
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact

// TODO(Testing): E2E execution boundaries.
// Tested natively via: `bazel test //riscv/test:riscv_smstateen_test`
// We acknowledge that Smstateen does not currently evaluate `ExceptionCode::kIllegalInstruction`
// across decoded boundaries. This constitutes a technical debt logging for the upcoming cycle.
