// Copyright 2025 Google LLC

#include "utils/assembler/native_assembler_wrapper.h"

#include <vector>
#include <cstdint>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace mpact::sim::assembler {
namespace {

class NativeTextualAssemblerIntegrationTest : public ::testing::Test {
 protected:
  NativeTextualAssembler assembler_;
};

TEST_F(NativeTextualAssemblerIntegrationTest, CanAssembleBasicInstruction) {
  auto res = assembler_.Assemble("addw x1, x2, x3");
  EXPECT_TRUE(res.ok());
  EXPECT_GE(res.value().size(), 4);
}

TEST_F(NativeTextualAssemblerIntegrationTest, RejectsMalformedStringWithoutAbort) {
  auto res = assembler_.Assemble("addw x1, x2, x3, x4");
  EXPECT_FALSE(res.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(res.status()) || absl::IsInternal(res.status()));
}

TEST_F(NativeTextualAssemblerIntegrationTest, EncodeInstructionWorks) {
  uint32_t encoded = 0;
  auto status = assembler_.EncodeInstruction("addw", {"x1", "x2", "x3"}, &encoded);
  EXPECT_TRUE(status.ok());
  // Verify deterministic payload for "addw x1, x2, x3" -> 0x003100BB
  EXPECT_EQ(encoded, 0x003100BB);
}

}  // namespace
}  // namespace mpact::sim::assembler
