#include "gtest/gtest.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_top.h"
#include "riscv/rva23u64_decoder_wrapper.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/util/memory/atomic_memory.h"
#include "mpact/sim/util/program_loader/elf_program_loader.h"

namespace {

using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVXlen;
using ::mpact::sim::riscv::RiscVTop;
using ::mpact::sim::riscv::Rva23u64DecoderWrapper;
using ::mpact::sim::util::AtomicMemory;
using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::util::ElfProgramLoader;

TEST(RiscVZawrsInstructionsTest, AuthenticE2EExecution) {
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_zawrs", RiscVXlen::RV64, memory, atomic_memory);
  
  auto* decoder = new Rva23u64DecoderWrapper(state, memory);
  auto* top = new RiscVTop("test_top", state, decoder);

  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/zawrs.elf");
  EXPECT_TRUE(load_result.ok());
  uint64_t entry_point = load_result.value();

  EXPECT_TRUE(top->WriteRegister("pc", entry_point).ok());
  auto status = top->Step(2); // Execute wrs.nto and wfi
  EXPECT_TRUE(status.ok());

  // PC should advance
  EXPECT_GT(top->ReadRegister("pc").value(), entry_point);

  delete top;
  delete decoder;
  delete state;
  delete atomic_memory;
  delete memory;
}

}  // namespace
