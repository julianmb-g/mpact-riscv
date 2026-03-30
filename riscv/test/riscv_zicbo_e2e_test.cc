#include "gtest/gtest.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_top.h"
#include "riscv/rva23u64_decoder_wrapper.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/util/memory/atomic_memory.h"
#include "utils/assembler/native_assembler_wrapper.h"

namespace {

using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVXlen;
using ::mpact::sim::riscv::RiscVTop;
using ::mpact::sim::riscv::Rva23u64DecoderWrapper;
using ::mpact::sim::util::AtomicMemory;
using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::assembler::NativeTextualAssembler;

class RiscVZicboE2ETest : public testing::Test {
 protected:
  RiscVZicboE2ETest() {
    memory_ = new FlatDemandMemory();
    atomic_memory_ = new AtomicMemory(memory_);
    state_ = new RiscVState("test_zicbo", RiscVXlen::RV64, memory_, atomic_memory_);
    decoder_ = new Rva23u64DecoderWrapper(state_, memory_);
    top_ = new RiscVTop("test_top", state_, decoder_);
  }

  ~RiscVZicboE2ETest() override {
    delete top_;
    delete decoder_;
    delete state_;
    delete atomic_memory_;
    delete memory_;
  }

  FlatDemandMemory* memory_;
  AtomicMemory* atomic_memory_;
  RiscVState* state_;
  Rva23u64DecoderWrapper* decoder_;
  RiscVTop* top_;
  NativeTextualAssembler assembler_;
};

TEST_F(RiscVZicboE2ETest, AuthenticCboZeroExecutionUnaligned) {
  uint32_t encoded = 0;
  auto status = assembler_.EncodeInstruction("cbo.zero", {"(x10)"}, &encoded);
  ASSERT_TRUE(status.ok()) << "Failed to encode cbo.zero: " << status.message();

  uint64_t entry_point = 0x1000;
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, encoded);
  memory_->Store(entry_point, db);
  db->DecRef();

  // Make memory initially dirty at 0x2000
  uint64_t base_addr = 0x2000;
  uint64_t target_addr = 0x2018; // Unaligned address!
  auto* dirty_db = state_->db_factory()->Allocate<uint8_t>(64);
  auto data = dirty_db->Get<uint8_t>();
  for (int i = 0; i < 64; ++i) {
    data[i] = 0xFF;
  }
  memory_->Store(base_addr, dirty_db);
  dirty_db->DecRef();

  // Set M-mode to bypass CBZE permissions
  state_->set_privilege_mode(mpact::sim::riscv::PrivilegeMode::kMachine);

  // Set pc and x10
  EXPECT_TRUE(top_->WriteRegister("pc", entry_point).ok());
  auto x10_reg = state_->GetRegister<mpact::sim::riscv::RV64Register>("x10").first;
  ASSERT_NE(x10_reg, nullptr);
  auto db_x10 = state_->db_factory()->Allocate<uint64_t>(1);
  db_x10->Set<uint64_t>(0, target_addr);
  x10_reg->SetDataBuffer(db_x10);
  db_x10->DecRef();

  auto step_status = top_->Step(1);
  EXPECT_TRUE(step_status.ok());

  // PC should advance by 4
  EXPECT_EQ(top_->ReadRegister("pc").value(), entry_point + 4);

  // Read memory at 0x2000 and verify it is zeroed (because it aligns the address)
  auto* read_db = state_->db_factory()->Allocate<uint8_t>(64);
  memory_->Load(base_addr, read_db, nullptr, nullptr);
  auto read_data = read_db->Get<uint8_t>();
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(read_data[i], 0) << "Memory at offset " << i << " was not zeroed despite unaligned address!";
  }
  read_db->DecRef();
}

TEST_F(RiscVZicboE2ETest, AuthenticCboCleanExecution) {
  uint32_t encoded = 0;
  auto status = assembler_.EncodeInstruction("cbo.clean", {"(x10)"}, &encoded);
  ASSERT_TRUE(status.ok()) << "Failed to encode cbo.clean: " << status.message();

  uint64_t entry_point = 0x1000;
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, encoded);
  memory_->Store(entry_point, db);
  db->DecRef();

  uint64_t target_addr = 0x2000;
  // Make memory initially dirty at 0x2000
  auto* dirty_db = state_->db_factory()->Allocate<uint8_t>(64);
  auto data = dirty_db->Get<uint8_t>();
  for (int i = 0; i < 64; ++i) {
    data[i] = 0xAB;
  }
  memory_->Store(target_addr, dirty_db);
  dirty_db->DecRef();

  state_->set_privilege_mode(mpact::sim::riscv::PrivilegeMode::kMachine);

  EXPECT_TRUE(top_->WriteRegister("pc", entry_point).ok());
  auto x10_reg = state_->GetRegister<mpact::sim::riscv::RV64Register>("x10").first;
  auto db_x10 = state_->db_factory()->Allocate<uint64_t>(1);
  db_x10->Set<uint64_t>(0, target_addr);
  x10_reg->SetDataBuffer(db_x10);
  db_x10->DecRef();

  auto step_status = top_->Step(1);
  EXPECT_TRUE(step_status.ok());
  EXPECT_EQ(top_->ReadRegister("pc").value(), entry_point + 4);

  // Read memory at 0x2000 and verify it is completely untouched by non-zeroing cache block ops
  auto* read_db = state_->db_factory()->Allocate<uint8_t>(64);
  memory_->Load(target_addr, read_db, nullptr, nullptr);
  auto read_data = read_db->Get<uint8_t>();
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(read_data[i], 0xAB) << "Memory at offset " << i << " was illegally mutated!";
  }
  read_db->DecRef();
}

TEST_F(RiscVZicboE2ETest, AuthenticCboFlushExecution) {
  uint32_t encoded = 0;
  auto status = assembler_.EncodeInstruction("cbo.flush", {"(x10)"}, &encoded);
  ASSERT_TRUE(status.ok()) << "Failed to encode cbo.flush: " << status.message();

  uint64_t entry_point = 0x1000;
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, encoded);
  memory_->Store(entry_point, db);
  db->DecRef();

  uint64_t target_addr = 0x2000;
  // Make memory initially dirty at 0x2000
  auto* dirty_db = state_->db_factory()->Allocate<uint8_t>(64);
  auto data = dirty_db->Get<uint8_t>();
  for (int i = 0; i < 64; ++i) {
    data[i] = 0xAB;
  }
  memory_->Store(target_addr, dirty_db);
  dirty_db->DecRef();

  state_->set_privilege_mode(mpact::sim::riscv::PrivilegeMode::kMachine);

  EXPECT_TRUE(top_->WriteRegister("pc", entry_point).ok());
  auto x10_reg = state_->GetRegister<mpact::sim::riscv::RV64Register>("x10").first;
  auto db_x10 = state_->db_factory()->Allocate<uint64_t>(1);
  db_x10->Set<uint64_t>(0, target_addr);
  x10_reg->SetDataBuffer(db_x10);
  db_x10->DecRef();

  auto step_status = top_->Step(1);
  EXPECT_TRUE(step_status.ok());
  EXPECT_EQ(top_->ReadRegister("pc").value(), entry_point + 4);

  // Read memory at 0x2000 and verify it is completely untouched by non-zeroing cache block ops
  auto* read_db = state_->db_factory()->Allocate<uint8_t>(64);
  memory_->Load(target_addr, read_db, nullptr, nullptr);
  auto read_data = read_db->Get<uint8_t>();
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(read_data[i], 0xAB) << "Memory at offset " << i << " was illegally mutated!";
  }
  read_db->DecRef();
}

TEST_F(RiscVZicboE2ETest, AuthenticCboInvalExecution) {
  uint32_t encoded = 0;
  auto status = assembler_.EncodeInstruction("cbo.inval", {"(x10)"}, &encoded);
  ASSERT_TRUE(status.ok()) << "Failed to encode cbo.inval: " << status.message();

  uint64_t entry_point = 0x1000;
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, encoded);
  memory_->Store(entry_point, db);
  db->DecRef();

  uint64_t target_addr = 0x2000;
  // Make memory initially dirty at 0x2000
  auto* dirty_db = state_->db_factory()->Allocate<uint8_t>(64);
  auto data = dirty_db->Get<uint8_t>();
  for (int i = 0; i < 64; ++i) {
    data[i] = 0xAB;
  }
  memory_->Store(target_addr, dirty_db);
  dirty_db->DecRef();

  state_->set_privilege_mode(mpact::sim::riscv::PrivilegeMode::kMachine);

  EXPECT_TRUE(top_->WriteRegister("pc", entry_point).ok());
  auto x10_reg = state_->GetRegister<mpact::sim::riscv::RV64Register>("x10").first;
  auto db_x10 = state_->db_factory()->Allocate<uint64_t>(1);
  db_x10->Set<uint64_t>(0, target_addr);
  x10_reg->SetDataBuffer(db_x10);
  db_x10->DecRef();

  auto step_status = top_->Step(1);
  EXPECT_TRUE(step_status.ok());
  EXPECT_EQ(top_->ReadRegister("pc").value(), entry_point + 4);

  // Read memory at 0x2000 and verify it is completely untouched by non-zeroing cache block ops
  auto* read_db = state_->db_factory()->Allocate<uint8_t>(64);
  memory_->Load(target_addr, read_db, nullptr, nullptr);
  auto read_data = read_db->Get<uint8_t>();
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(read_data[i], 0xAB) << "Memory at offset " << i << " was illegally mutated!";
  }
  read_db->DecRef();
}

TEST_F(RiscVZicboE2ETest, TrapOnUnprivilegedCboZero) {
  uint32_t encoded = 0;
  auto status = assembler_.EncodeInstruction("cbo.zero", {"(x10)"}, &encoded);
  ASSERT_TRUE(status.ok());

  uint64_t entry_point = 0x1000;
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, encoded);
  memory_->Store(entry_point, db);
  db->DecRef();

  // Ensure trap is routed through CPU loop naturally
  bool trap_taken = false;
  state_->set_on_trap(
      [&trap_taken](bool is_interrupt, uint64_t trap_value,
                    uint64_t exception_code, uint64_t epc,
                    const mpact::sim::riscv::Instruction* inst) -> bool {
        trap_taken = true;
        EXPECT_EQ(exception_code, static_cast<uint64_t>(mpact::sim::riscv::ExceptionCode::kIllegalInstruction));
        return true;
      });

  // U-mode without menvcfg/senvcfg CBZE = 1 should trap
  state_->set_privilege_mode(mpact::sim::riscv::PrivilegeMode::kUser);
  auto res_m = state_->csr_set()->GetCsr(static_cast<uint64_t>(mpact::sim::riscv::RiscVCsrEnum::kMenvcfg));
  ASSERT_TRUE(res_m.ok());
  res_m.value()->Write(static_cast<uint64_t>(0));

  EXPECT_TRUE(top_->WriteRegister("pc", entry_point).ok());
  auto x10_reg = state_->GetRegister<mpact::sim::riscv::RV64Register>("x10").first;
  auto db_x10 = state_->db_factory()->Allocate<uint64_t>(1);
  db_x10->Set<uint64_t>(0, 0x2000);
  x10_reg->SetDataBuffer(db_x10);
  db_x10->DecRef();

  auto step_status = top_->Step(1);
  EXPECT_TRUE(step_status.ok()); // Step returns OK even if it traps internally
  EXPECT_TRUE(trap_taken) << "Instruction did not trap when unprivileged!";
}

}  // namespace