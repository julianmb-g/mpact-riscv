#include "riscv/rva23u64_decoder_wrapper.h"
#include "riscv/rva23s64_decoder_wrapper.h"

#include <cstdint>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/log_sink_registry.h"
#include "absl/strings/str_cat.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/instruction.h"
#include "riscv/riscv_state.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"

namespace {

using ::mpact::sim::generic::DataBuffer;
using ::mpact::sim::generic::DataBufferFactory;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVXlen;
using ::mpact::sim::riscv::Rva23u64DecoderWrapper;
using ::mpact::sim::riscv::Rva23s64DecoderWrapper;
using ::mpact::sim::util::FlatDemandMemory;

class Rva23InstructionDecoderTest : public testing::Test {
 protected:
  Rva23InstructionDecoderTest() {
    mem_ = new FlatDemandMemory();
    state_ = new RiscVState("test", RiscVXlen::RV64, mem_);
  }

  ~Rva23InstructionDecoderTest() override {
    delete state_;
    delete mem_;
  }

  RiscVState* state_;
  FlatDemandMemory* mem_;
  DataBufferFactory db_factory_;
};

TEST_F(Rva23InstructionDecoderTest, BasicDecoding) {
  // Test that both the user and supervisor RVA23 decoders instantiate properly.
  auto* u_decoder = new Rva23u64DecoderWrapper(state_, mem_);
  auto* s_decoder = new Rva23s64DecoderWrapper(state_, mem_);

  // Write a simple NOP instruction (0x00000013) to memory at 0x1000.
  auto* db = db_factory_.Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, 0x00000013);
  mem_->Store(0x1000, db);
  db->DecRef();

  Instruction* u_inst = u_decoder->DecodeInstruction(0x1000);
  Instruction* s_inst = s_decoder->DecodeInstruction(0x1000);

  EXPECT_NE(u_inst, nullptr);
  EXPECT_NE(s_inst, nullptr);

  u_inst->DecRef();
  s_inst->DecRef();
  
  delete u_decoder;
  delete s_decoder;
}

}  // namespace
