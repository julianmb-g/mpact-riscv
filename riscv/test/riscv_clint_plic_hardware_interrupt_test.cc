// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>

#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/counters.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "absl/status/status.h"
#include "absl/log/check.h"
#include "riscv/riscv32_decoder.h"
#include "riscv/riscv_clint.h"
#include "riscv/riscv_plic.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_top.h"
#include "utils/assembler/native_assembler_wrapper.h"

namespace {

using ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::SimpleCounter;
using ::mpact::sim::riscv::RiscV32Decoder;
using ::mpact::sim::riscv::RiscVClint;
using ::mpact::sim::riscv::RiscVPlic;
using ::mpact::sim::riscv::RiscVPlicIrqInterface;
using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVTop;
using ::mpact::sim::riscv::RiscVXlen;
using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::assembler::NativeTextualAssembler;

constexpr uint64_t kMTimeCmp = 0x4000;

class PlicToCpuIrqAdapter : public RiscVPlicIrqInterface {
 public:
  explicit PlicToCpuIrqAdapter(RiscVState* state) : state_(state) {}
  void SetIrq(bool irq_value) override {
    state_->mip()->set_meip(irq_value);
    state_->CheckForInterrupt();
  }
 private:
  RiscVState* state_;
};

class ClintPlicHardwareInterruptTest : public ::testing::Test {
 protected:
  ClintPlicHardwareInterruptTest() {
    memory_ = new FlatDemandMemory();
    state_ = new RiscVState("test_state", RiscVXlen::RV32, memory_);
    decoder_ = new RiscV32Decoder(state_, memory_);
    riscv_top_ = new RiscVTop("test_top", state_, decoder_);

    // Hook up Trap Handler
    state_->set_on_trap([this](bool is_interrupt, uint64_t trap_value,
                               uint64_t exception_code, uint64_t epc,
                               const Instruction* inst) {
      trap_taken_ = true;
      trap_is_interrupt_ = is_interrupt;
      trap_exception_code_ = exception_code;
      trap_epc_ = epc;
      return true;
    });

    // Cycle Counter for CLINT
    cycle_counter_.Initialize("cycle_counter", 0);

    // Instantiate PLIC (2 sources: 0 is null, 1 is valid, 1 context)
    plic_ = new RiscVPlic(2, 1);
    plic_adapter_ = new PlicToCpuIrqAdapter(state_);
    plic_->SetContext(0, plic_adapter_);
    CHECK_OK(plic_->Configure("0=0;1=1;", "0=0,1,1;"));

    // Instantiate CLINT
    clint_ = new RiscVClint(/*period=*/1, state_->mip());
    cycle_counter_.AddListener(clint_);

    // Set MTimeCmp to a large value initially so MTIP doesn't fire immediately
    auto* mtimecmp_db = state_->db_factory()->Allocate<uint64_t>(1);
    mtimecmp_db->Set<uint64_t>(0, 0xFFFFFFFF);
    clint_->Store(kMTimeCmp, mtimecmp_db);
    mtimecmp_db->DecRef();

    // Setup RiscV CPU state
    // Enable Machine External (11), Machine Timer (7), and Machine Software (3) in MIE
    state_->mie()->set_meie(1);
    state_->mie()->set_mtie(1);
    state_->mie()->set_msie(1);
    // Enable machine interrupts globally: MSTATUS.MIE=1 (bit 3)
    auto status = riscv_top_->WriteRegister("mstatus", 0x8);
    EXPECT_TRUE(status.ok());

    // Setup Authentic Payload using Assembler
    NativeTextualAssembler assembler;
    uint32_t nop_opcode = 0;
    EXPECT_TRUE(assembler.EncodeInstruction("nop", {}, &nop_opcode).ok());
    uint32_t wfi_opcode = 0;
    EXPECT_TRUE(assembler.EncodeInstruction("wfi", {}, &wfi_opcode).ok());
    
    // Store instructions starting at 0x1000
    auto* db = state_->db_factory()->Allocate<uint32_t>(1);
    db->Set<uint32_t>(0, nop_opcode);
    memory_->Store(0x1000, db);
    memory_->Store(0x1004, db);
    db->Set<uint32_t>(0, wfi_opcode);
    memory_->Store(0x1008, db);
    db->DecRef();

    EXPECT_TRUE(riscv_top_->WriteRegister("pc", 0x1000).ok());
  }

  ~ClintPlicHardwareInterruptTest() override {
    delete clint_;
    delete plic_adapter_;
    delete plic_;
    delete riscv_top_;
    delete decoder_;
    delete state_;
    delete memory_;
  }

  FlatDemandMemory* memory_;
  RiscV32Decoder* decoder_;
  RiscVTop* riscv_top_;
  RiscVState* state_;
  RiscVPlic* plic_;
  RiscVClint* clint_;
  PlicToCpuIrqAdapter* plic_adapter_;
  SimpleCounter<uint64_t> cycle_counter_;

  bool trap_taken_ = false;
  bool trap_is_interrupt_ = false;
  uint64_t trap_exception_code_ = 0;
  uint64_t trap_epc_ = 0;
};

// Test PLIC Machine External Interrupt (Exception Code 11)
TEST_F(ClintPlicHardwareInterruptTest, PlicMachineExternalInterruptDelivery) {
  auto res = riscv_top_->Step(2); // Execute nop, nop
  EXPECT_TRUE(res.ok());
  EXPECT_EQ(state_->pc_operand()->AsUint32(0), 0x1008);
  EXPECT_FALSE(trap_taken_);

  // PLIC triggers external interrupt
  plic_->SetInterrupt(1, true, true);
  EXPECT_TRUE(state_->is_interrupt_available());
  
  // Step WFI and immediately trap
  res = riscv_top_->Step(1);
  EXPECT_TRUE(res.ok());
  
  EXPECT_TRUE(trap_taken_);
  EXPECT_TRUE(trap_is_interrupt_);
  EXPECT_EQ(trap_exception_code_, 11); // Machine External Interrupt
  EXPECT_EQ(trap_epc_, 0x100C);
}

// Test CLINT Machine Timer Interrupt (Exception Code 7)
TEST_F(ClintPlicHardwareInterruptTest, ClintMachineTimerInterruptDelivery) {
  auto res = riscv_top_->Step(2); // Execute nop, nop
  EXPECT_TRUE(res.ok());
  EXPECT_EQ(state_->pc_operand()->AsUint32(0), 0x1008);
  EXPECT_FALSE(trap_taken_);

  // Write a tiny MTimeCmp target via CLINT memory interface
  auto* db = state_->db_factory()->Allocate<uint64_t>(1);
  db->Set<uint64_t>(0, 10);
  clint_->Store(kMTimeCmp, db);
  db->DecRef();

  // Advance time beyond the compare threshold via counter increments
  for (int i = 0; i < 20; ++i) {
    cycle_counter_.Increment(1);
  }
  
  // Interrupt should now be pending from CLINT
  EXPECT_TRUE(state_->is_interrupt_available());
  
  // Step WFI and immediately trap
  res = riscv_top_->Step(1);
  EXPECT_TRUE(res.ok());
  
  EXPECT_TRUE(trap_taken_);
  EXPECT_TRUE(trap_is_interrupt_);
  EXPECT_EQ(trap_exception_code_, 7); // Machine Timer Interrupt
  EXPECT_EQ(trap_epc_, 0x100C);
}

// Test CLINT Machine Software Interrupt (Exception Code 3)
TEST_F(ClintPlicHardwareInterruptTest, ClintSoftwareInterruptDelivery) {
  auto res = riscv_top_->Step(2); // Execute nop, nop
  EXPECT_TRUE(res.ok());
  EXPECT_EQ(state_->pc_operand()->AsUint32(0), 0x1008);
  EXPECT_FALSE(trap_taken_);

  // Write 1 to MSIP (Machine Software Interrupt Pending) register at offset 0
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, 1);
  clint_->Store(0x0, db);
  db->DecRef();

  // Interrupt should now be pending from CLINT
  EXPECT_TRUE(state_->is_interrupt_available());
  
  // Step WFI and immediately trap
  res = riscv_top_->Step(1);
  EXPECT_TRUE(res.ok());
  
  EXPECT_TRUE(trap_taken_);
  EXPECT_TRUE(trap_is_interrupt_);
  EXPECT_EQ(trap_exception_code_, 3); // Machine Software Interrupt
  EXPECT_EQ(trap_epc_, 0x100C);
}

}  // namespace
