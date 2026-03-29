// Copyright 2026 Google LLC
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include "gtest/gtest.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "riscv/riscv64gv_bin_encoder.h"
#include "riscv/rva23u64_decoder_wrapper.h"
#include "riscv/riscv_register.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_vector_state.h"
#include "riscv/riscv_fp_state.h"
#include "riscv/riscv_top.h"
#include "utils/assembler/native_assembler_wrapper.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace {

using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::Rva23u64DecoderWrapper;
using ::mpact::sim::riscv::RiscVVectorState;
using ::mpact::sim::riscv::RiscVFPState;
using ::mpact::sim::riscv::RiscVTop;
using ::mpact::sim::riscv::RV64Register;
using ::mpact::sim::riscv::RVFpRegister;
using ::mpact::sim::assembler::NativeTextualAssembler;
using ::mpact::sim::util::FlatDemandMemory;

class VectorFpOrganicTest : public ::testing::Test {
 protected:
  VectorFpOrganicTest() {
    memory_ = new FlatDemandMemory();
    state_ = new RiscVState("test_vector_fp", RiscVXlen::RV64, memory_);
    fp_state_ = new RiscVFPState(state_->csr_set(), state_);
    state_->set_rv_fp(fp_state_);
    vector_state_ = new RiscVVectorState(state_, 64);
    state_->set_rv_vector(vector_state_);
    decoder_ = new Rva23u64DecoderWrapper(state_, memory_);
    std::string reg_name;
    for (int i = 0; i < 32; i++) {
      reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
      state_->AddRegister<RV64Register>(reg_name);
    }
    for (int i = 0; i < 32; i++) {
      reg_name = absl::StrCat(RiscVState::kFregPrefix, i);
      state_->AddRegister<RVFpRegister>(reg_name);
    }
    top_ = new RiscVTop("test_top", state_, decoder_);
  }
  ~VectorFpOrganicTest() override {
    delete top_;
    delete decoder_;
    delete state_;
    delete memory_;
  }
  FlatDemandMemory* memory_;
  RiscVState* state_;
  RiscVFPState* fp_state_;
  RiscVVectorState* vector_state_;
  Rva23u64DecoderWrapper* decoder_;
  RiscVTop* top_;
};

TEST_F(VectorFpOrganicTest, OrganicVectorFloatingPointAddition) {
  NativeTextualAssembler assembler;
  auto elf_bytes = assembler.Assemble("nop\n");
  ASSERT_TRUE(elf_bytes.ok());
  std::vector<uint8_t> payload = elf_bytes.value();

  // lui t0, 6
  uint32_t lui_t0 = 0;
  lui_t0 = encoding64::Encoder::UType::InsertOpcode(0x37, lui_t0);
  lui_t0 = encoding64::Encoder::UType::InsertRd(5, lui_t0);
  lui_t0 = encoding64::Encoder::UType::InsertImm20(6, lui_t0);
  
  // addiw t0, t0, 1536 (0x600)
  uint32_t addi_t0 = 0;
  addi_t0 = encoding64::Encoder::IType::InsertOpcode(0x1b, addi_t0); // addiw
  addi_t0 = encoding64::Encoder::IType::InsertFunc3(0x0, addi_t0);
  addi_t0 = encoding64::Encoder::IType::InsertRd(5, addi_t0);
  addi_t0 = encoding64::Encoder::IType::InsertRs1(5, addi_t0);
  addi_t0 = encoding64::Encoder::IType::InsertImm12(1536, addi_t0);

  // csrs mstatus, t0
  uint32_t csrs = 0;
  csrs = encoding64::Encoder::IType::InsertOpcode(0x73, csrs);
  csrs = encoding64::Encoder::IType::InsertFunc3(0x2, csrs); // csrs
  csrs = encoding64::Encoder::IType::InsertRd(0, csrs); // zero
  csrs = encoding64::Encoder::IType::InsertRs1(5, csrs); // t0
  csrs = encoding64::Encoder::IType::InsertImm12(0x300, csrs); // mstatus

  // lui t1, 263168 (0x40400)
  uint32_t lui_t1 = 0;
  lui_t1 = encoding64::Encoder::UType::InsertOpcode(0x37, lui_t1);
  lui_t1 = encoding64::Encoder::UType::InsertRd(6, lui_t1); // t1
  lui_t1 = encoding64::Encoder::UType::InsertImm20(0x40400, lui_t1);

  // lui t3, 2
  uint32_t lui_t3 = 0;
  lui_t3 = encoding64::Encoder::UType::InsertOpcode(0x37, lui_t3);
  lui_t3 = encoding64::Encoder::UType::InsertRd(28, lui_t3); // t3
  lui_t3 = encoding64::Encoder::UType::InsertImm20(2, lui_t3);

  // fmv.w.x f1, t1
  uint32_t fmv_f1 = 0;
  fmv_f1 = encoding64::Encoder::RType::InsertOpcode(0x53, fmv_f1);
  fmv_f1 = encoding64::Encoder::RType::InsertFunc3(0x0, fmv_f1);
  fmv_f1 = encoding64::Encoder::RType::InsertFunc7(0x78, fmv_f1); 
  fmv_f1 = encoding64::Encoder::RType::InsertRd(1, fmv_f1); // f1
  fmv_f1 = encoding64::Encoder::RType::InsertRs1(6, fmv_f1); // t1=x6
  fmv_f1 = encoding64::Encoder::RType::InsertRs2(0, fmv_f1); 

  // fmv.w.x f2, t1
  uint32_t fmv_f2 = 0;
  fmv_f2 = encoding64::Encoder::RType::InsertOpcode(0x53, fmv_f2);
  fmv_f2 = encoding64::Encoder::RType::InsertFunc3(0x0, fmv_f2);
  fmv_f2 = encoding64::Encoder::RType::InsertFunc7(0x78, fmv_f2); 
  fmv_f2 = encoding64::Encoder::RType::InsertRd(2, fmv_f2); // f2
  fmv_f2 = encoding64::Encoder::RType::InsertRs1(6, fmv_f2); // t1=x6
  fmv_f2 = encoding64::Encoder::RType::InsertRs2(0, fmv_f2); 

  uint32_t vsetvli = 0;
  vsetvli = encoding64::Encoder::VConfig::InsertOpcode(0x57, vsetvli);
  vsetvli = encoding64::Encoder::VConfig::InsertFunc3(0x7, vsetvli);
  vsetvli = encoding64::Encoder::VConfig::InsertRd(5, vsetvli); // t0=x5
  vsetvli = encoding64::Encoder::VConfig::InsertRs1(0, vsetvli); // x0
  vsetvli = encoding64::Encoder::VConfig::InsertZimm11(0xD0, vsetvli); 
  
  uint32_t vfmv_v1 = 0; // vfmv.v.f v1, f1
  vfmv_v1 = encoding64::Encoder::VArith::InsertOpcode(0x57, vfmv_v1);
  vfmv_v1 = encoding64::Encoder::VArith::InsertFunc3(0x5, vfmv_v1);
  vfmv_v1 = encoding64::Encoder::VArith::InsertFunc6(0x17, vfmv_v1); // 0b010111
  vfmv_v1 = encoding64::Encoder::VArith::InsertRs1(1, vfmv_v1); // f1
  vfmv_v1 = encoding64::Encoder::VArith::InsertVd(1, vfmv_v1); // v1
  vfmv_v1 = encoding64::Encoder::VArith::InsertVm(1, vfmv_v1); 
  vfmv_v1 = encoding64::Encoder::VArith::InsertVs2(0, vfmv_v1); // must be 0
  
  uint32_t vfmv_v2 = 0; // vfmv.v.f v2, f2
  vfmv_v2 = encoding64::Encoder::VArith::InsertOpcode(0x57, vfmv_v2);
  vfmv_v2 = encoding64::Encoder::VArith::InsertFunc3(0x5, vfmv_v2);
  vfmv_v2 = encoding64::Encoder::VArith::InsertFunc6(0x17, vfmv_v2);
  vfmv_v2 = encoding64::Encoder::VArith::InsertRs1(2, vfmv_v2); // f2
  vfmv_v2 = encoding64::Encoder::VArith::InsertVd(2, vfmv_v2); // v2
  vfmv_v2 = encoding64::Encoder::VArith::InsertVm(1, vfmv_v2);
  vfmv_v2 = encoding64::Encoder::VArith::InsertVs2(0, vfmv_v2);

  uint32_t vfadd = 0; // vfadd.vv v3, v1, v2
  vfadd = encoding64::Encoder::VArith::InsertOpcode(0x57, vfadd);
  vfadd = encoding64::Encoder::VArith::InsertFunc3(0x1, vfadd);
  vfadd = encoding64::Encoder::VArith::InsertFunc6(0x0, vfadd);
  vfadd = encoding64::Encoder::VArith::InsertRs1(1, vfadd); // v1 (vs1)
  vfadd = encoding64::Encoder::VArith::InsertVs2(2, vfadd); // v2 (vs2)
  vfadd = encoding64::Encoder::VArith::InsertVd(3, vfadd); // v3 (vd)
  vfadd = encoding64::Encoder::VArith::InsertVm(1, vfadd);

  uint32_t vse32 = 0; // vse32.v v3, (t3)
  vse32 = encoding64::Encoder::VMem::InsertOpcode(0x27, vse32);
  vse32 = encoding64::Encoder::VMem::InsertWidth(0x6, vse32); // e32 width
  vse32 = encoding64::Encoder::VMem::InsertNf(0, vse32);
  vse32 = encoding64::Encoder::VMem::InsertMew(0, vse32);
  vse32 = encoding64::Encoder::VMem::InsertMop(0, vse32);
  vse32 = encoding64::Encoder::VMem::InsertSumop(0, vse32);
  vse32 = encoding64::Encoder::VMem::InsertRs1(28, vse32); // t3 is x28
  vse32 = encoding64::Encoder::VMem::InsertVs3(3, vse32); // v3
  vse32 = encoding64::Encoder::VMem::InsertVm(1, vse32);

  auto add_inst = [&](uint32_t inst) {
    payload.push_back(inst & 0xff);
    payload.push_back((inst >> 8) & 0xff);
    payload.push_back((inst >> 16) & 0xff);
    payload.push_back((inst >> 24) & 0xff);
  };
  add_inst(lui_t0);
  add_inst(addi_t0);
  add_inst(csrs);
  add_inst(lui_t1);
  add_inst(lui_t3);
  add_inst(fmv_f1);
  add_inst(fmv_f2);
  add_inst(vsetvli);
  add_inst(vfmv_v1);
  add_inst(vfmv_v2);
  add_inst(vfadd);
  add_inst(vse32);

  while (payload.size() % 4 != 0) { payload.push_back(0); }

  auto* db = state_->db_factory()->Allocate<uint8_t>(payload.size());
  std::memcpy(db->raw_ptr(), payload.data(), payload.size());
  memory_->Store(0x1000, db);
  db->DecRef();

  ASSERT_TRUE(top_->WriteRegister("pc", 0x1000).ok());

  // 1 NOP + 12 explicit = 13 steps
  EXPECT_TRUE(top_->Step(13).ok());

  auto* out_db = state_->db_factory()->Allocate<uint32_t>(1);
  memory_->Load(0x2000, out_db, nullptr, nullptr);
  EXPECT_EQ(out_db->Get<uint32_t>(0), 0x40C00000) << "Vector FP Math failed to write correctly to memory.";
  out_db->DecRef();
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
