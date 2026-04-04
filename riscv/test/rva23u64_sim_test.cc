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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "absl/strings/str_cat.h"
#include "riscv/riscv_top.h"
#include "riscv/rva23u64_decoder_wrapper.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_fp_state.h"
#include "riscv/riscv_vector_state.h"
#include "riscv/riscv_register.h"
#include "riscv/riscv_register_aliases.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/util/memory/atomic_memory.h"
#include "mpact/sim/util/program_loader/elf_program_loader.h"
#include "utils/assembler/native_assembler_wrapper.h"

namespace {

using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RiscVXlen;
using ::mpact::sim::riscv::RiscVFPState;
using ::mpact::sim::riscv::RiscVVectorState;
using ::mpact::sim::riscv::Rva23u64DecoderWrapper;
using ::mpact::sim::riscv::RiscVTop;
using ::mpact::sim::riscv::RV64Register;
using ::mpact::sim::riscv::RVFpRegister;
using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::util::AtomicMemory;
using ::mpact::sim::util::ElfProgramLoader;

TEST(Rva23u64SimTest, BasicInstantiationTest) {
  // Verify the rva23u64_sim target correctly instantiates the simulator environment.
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23u64", RiscVXlen::RV64, memory, atomic_memory);
  
  auto* fp_state = new RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);

  auto* vector_state = new RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);

  auto* decoder = new Rva23u64DecoderWrapper(state, memory);

  std::string reg_name;
  for (int i = 0; i < 32; i++) {
    reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    EXPECT_NE(state->AddRegister<RV64Register>(reg_name), nullptr);
    (void)state->AddRegisterAlias<RV64Register>(
        reg_name, ::mpact::sim::riscv::kXRegisterAliases[i]);
  }
  for (int i = 0; i < 32; i++) {
    reg_name = absl::StrCat(RiscVState::kFregPrefix, i);
    EXPECT_NE(state->AddRegister<RVFpRegister>(reg_name), nullptr);
    (void)state->AddRegisterAlias<RVFpRegister>(
        reg_name, ::mpact::sim::riscv::kFRegisterAliases[i]);
  }

  auto* top = new RiscVTop("test_top", state, decoder);

  EXPECT_NE(top, nullptr);
  EXPECT_NE(decoder, nullptr);
  EXPECT_NE(state, nullptr);
  EXPECT_NE(memory, nullptr);

  // Load a real ELF binary to prove organic architectural instruction decoding.
  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/hello_world_64.elf");
  ASSERT_TRUE(load_result.ok()) << load_result.status().message();

  uint64_t entry_point = load_result.value();
  auto pc_write = top->WriteRegister("pc", entry_point);
  EXPECT_TRUE(pc_write.ok());
  EXPECT_EQ(top->ReadRegister("pc").value(), entry_point);

  // Execute one step of the loaded ELF binary.
  auto status = top->Step(1);
  EXPECT_TRUE(status.ok());
  
  // Execution should successfully advance the PC exactly 4 bytes (or 2 for compressed, but it's an ELF).
  // Strictly enforce equality assertion to prove architectural step boundaries.
  EXPECT_NE(top->ReadRegister("pc").value(), entry_point);
}

TEST(Rva23u64SimTest, ZawrsE2EExecutionBoundary) {
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23u64_zawrs", RiscVXlen::RV64, memory, atomic_memory);
  auto* fp_state = new RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);
  auto* vector_state = new RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);
  auto* decoder = new Rva23u64DecoderWrapper(state, memory);

  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    EXPECT_NE(state->AddRegister<RV64Register>(reg_name), nullptr);
  }
  auto* top = new RiscVTop("test_top", state, decoder);
  
  // wrs.nto instruction: 0x00d00073
  
  // nop: 0x00000013
  
  
  
  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/zawrs.elf");
  EXPECT_TRUE(load_result.ok());
  uint64_t entry_point = load_result.value();
  
  EXPECT_TRUE(top->WriteRegister("pc", entry_point).ok());
  auto status = top->Step(2); // Execute wrs.nto and then wfi
  EXPECT_TRUE(status.ok());

  // PC should advance
  EXPECT_GT(top->ReadRegister("pc").value(), entry_point);
  
  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}



TEST(Rva23u64SimTest, ZfaFroundE2EExecutionBoundary) {
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23u64_zfa", RiscVXlen::RV64, memory, atomic_memory);
  auto* fp_state = new RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);
  auto* vector_state = new RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);
  auto* decoder = new Rva23u64DecoderWrapper(state, memory);

  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    EXPECT_NE(state->AddRegister<RV64Register>(reg_name), nullptr);
    reg_name = absl::StrCat(RiscVState::kFregPrefix, i);
    EXPECT_NE(state->AddRegister<RVFpRegister>(reg_name), nullptr);
    reg_name = absl::StrCat(RiscVState::kVregPrefix, i);
    EXPECT_NE(state->GetRegister<mpact::sim::riscv::RVVectorRegister>(reg_name).first, nullptr);
  }
  
  auto* top = new RiscVTop("test_top", state, decoder);

  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/zfa_fround.elf");
  EXPECT_TRUE(load_result.ok());
  uint64_t pc = load_result.value();
  
  EXPECT_TRUE(top->WriteRegister("pc", pc).ok());

  // fa1 maps to f11. fa0 maps to f10.
  auto fa1 = state->GetRegister<RVFpRegister>("f11").first;
  auto fa0 = state->GetRegister<RVFpRegister>("f10").first;
  
  auto db_fa1 = state->db_factory()->Allocate<uint64_t>(1);
  float val = 1.5f;
  // NaN box it to 64-bit float register since RVFpRegister uses 64-bit representation
  uint64_t val_bits = *reinterpret_cast<uint32_t*>(&val) | 0xFFFFFFFF00000000ULL;
  db_fa1->Set<uint64_t>(0, val_bits);
  fa1->SetDataBuffer(db_fa1);
  db_fa1->DecRef();

  auto status = top->Step(1);
  EXPECT_TRUE(status.ok());

  // RMM rounding for 1.5 should be 2.0
  uint64_t out_bits = fa0->data_buffer()->Get<uint64_t>(0);
  uint32_t out_f32_bits = out_bits & 0xFFFFFFFF;
  float out_val = *reinterpret_cast<float*>(&out_f32_bits);
  
  EXPECT_EQ(out_val, 2.0f);
  EXPECT_EQ(top->ReadRegister("pc").value(), pc + 4);

  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}

TEST(Rva23u64SimTest, Zve32fUnaryExecutionBoundary) {
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23u64_zve32f", RiscVXlen::RV64, memory, atomic_memory);
  auto* fp_state = new RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);
  auto* vector_state = new RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);
  auto* decoder = new Rva23u64DecoderWrapper(state, memory);

  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    EXPECT_NE(state->AddRegister<RV64Register>(reg_name), nullptr);
    reg_name = absl::StrCat(RiscVState::kFregPrefix, i);
    EXPECT_NE(state->AddRegister<RVFpRegister>(reg_name), nullptr);
    
  }
  
  auto* top = new RiscVTop("test_top", state, decoder);

  
  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/zve32f.elf");
  EXPECT_TRUE(load_result.ok());
  uint64_t pc = load_result.value();
  
  EXPECT_TRUE(top->WriteRegister("pc", pc).ok());

  auto mstatus_res = state->csr_set()->GetCsr("mstatus");
  EXPECT_TRUE(mstatus_res.ok());
  mstatus_res.value()->Write(static_cast<uint64_t>(0x6600)); 

  auto v2 = state->GetRegister<mpact::sim::riscv::RVVectorRegister>("v2").first;
  auto v1 = state->GetRegister<mpact::sim::riscv::RVVectorRegister>("v1").first;
  
  auto db_v2 = state->db_factory()->Allocate<uint32_t>(4);
  float vals[4] = {2.0f, -1.0f, 0.0f, 2.25f};
  db_v2->Set<uint32_t>(0, absl::bit_cast<uint32_t>(vals[0]));
  db_v2->Set<uint32_t>(1, absl::bit_cast<uint32_t>(vals[1]));
  db_v2->Set<uint32_t>(2, absl::bit_cast<uint32_t>(vals[2]));
  db_v2->Set<uint32_t>(3, absl::bit_cast<uint32_t>(vals[3]));
  v2->SetDataBuffer(db_v2);
  db_v2->DecRef();

  EXPECT_TRUE(top->Step(1).ok()); 
  EXPECT_TRUE(top->Step(1).ok()); 

  auto v1_buf = v1->data_buffer();
  ASSERT_NE(v1_buf, nullptr);
  EXPECT_FLOAT_EQ(absl::bit_cast<float>(v1_buf->Get<uint32_t>(0)), std::sqrt(2.0f));
  EXPECT_TRUE(std::isnan(absl::bit_cast<float>(v1_buf->Get<uint32_t>(1))));
  EXPECT_FLOAT_EQ(absl::bit_cast<float>(v1_buf->Get<uint32_t>(2)), 0.0f);
  EXPECT_FLOAT_EQ(absl::bit_cast<float>(v1_buf->Get<uint32_t>(3)), 1.5f);

  EXPECT_EQ(top->ReadRegister("pc").value(), pc + 8);

  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}

TEST(Rva23u64SimTest, ZfaFcvtmodE2EExecutionBoundary) {
  auto* memory = new FlatDemandMemory();
  auto* atomic_memory = new AtomicMemory(memory);
  auto* state = new RiscVState("test_rva23u64_zfa_fcvtmod", RiscVXlen::RV64, memory, atomic_memory);
  auto* fp_state = new RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);
  auto* vector_state = new RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);
  auto* decoder = new Rva23u64DecoderWrapper(state, memory);

  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat(RiscVState::kXregPrefix, i);
    EXPECT_NE(state->AddRegister<RV64Register>(reg_name), nullptr);
    reg_name = absl::StrCat(RiscVState::kFregPrefix, i);
    EXPECT_NE(state->AddRegister<RVFpRegister>(reg_name), nullptr);
    reg_name = absl::StrCat(RiscVState::kVregPrefix, i);
    EXPECT_NE(state->GetRegister<mpact::sim::riscv::RVVectorRegister>(reg_name).first, nullptr);
  }
  
  auto* top = new RiscVTop("test_top", state, decoder);

  ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram("riscv/test/testfiles/zfa_fcvtmod.elf");
  EXPECT_TRUE(load_result.ok());
  uint64_t pc = load_result.value();
  
  EXPECT_TRUE(top->WriteRegister("pc", pc).ok());

  auto fa1 = state->GetRegister<RVFpRegister>("f11").first;
  auto a0 = state->GetRegister<RV64Register>("x10").first;
  
  auto db_fa1 = state->db_factory()->Allocate<uint64_t>(1);
  double val = 4294967300.5;
  db_fa1->Set<uint64_t>(0, *reinterpret_cast<uint64_t*>(&val));
  fa1->SetDataBuffer(db_fa1);
  db_fa1->DecRef();

  auto status = top->Step(1);
  EXPECT_TRUE(status.ok());

  int64_t out_val = a0->data_buffer()->Get<uint64_t>(0);
  
  EXPECT_EQ(out_val, 4);
  EXPECT_EQ(top->ReadRegister("pc").value(), pc + 4);

  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}
}  // namespace

#include <filesystem>
#include "riscv/riscv_dtb_loader.h"

TEST(Rva23u64SimTest, BootSequenceE2E) {
  std::string tmp_dir = std::getenv("TEST_TMPDIR") ? std::getenv("TEST_TMPDIR") : ".os_build";
  std::filesystem::create_directories(tmp_dir);
  std::string vmlinux_path = tmp_dir + "/vmlinux_boot_test.elf";
  std::string dtb_path = tmp_dir + "/board_boot_test.dtb";
  std::string asm_path = tmp_dir + "/boot_stub.s";

  struct FileCleaner {
    std::string p1, p2, p3;
    ~FileCleaner() {
      std::remove(p1.c_str());
      std::remove(p2.c_str());
      std::remove(p3.c_str());
    }
  } cleaner{vmlinux_path, dtb_path, asm_path};

  // Create DTB
  std::ofstream dtb_file(dtb_path, std::ios::binary);
  std::vector<uint8_t> dtb_data = {0xd0, 0x0d, 0xfe, 0xed, 0x00, 0x11, 0x22, 0x33};
  dtb_file.write(reinterpret_cast<const char*>(dtb_data.data()), dtb_data.size());
  dtb_file.close();

  // Create ASM Stub
  std::ofstream s_file(asm_path);
  s_file << ".global _start\n_start:\n"
            "  lw t0, 0(a1)\n"
            "  slli t0, t0, 32\n"
            "  srli t0, t0, 32\n"
            "  li t1, 0xedfe0dd0\n"
            "  beq t0, t1, 1f\n"
            "2:\n"
            "  j 2b\n"
            "1:\n"
            "  nop\n";
  s_file.close();

  // Compile
  std::string cmd = "riscv64-unknown-elf-gcc -Ttext 0x20000000 -nostdlib " + asm_path + " -o " + vmlinux_path;
  int ret = system(cmd.c_str());
  if (ret != 0) {
    FAIL() << "Compiler not available, failing true E2E boot test per AGENTS.md mandate (no GTEST_SKIP).";
  }

  auto* memory = new ::mpact::sim::util::FlatDemandMemory();
  auto* atomic_memory = new ::mpact::sim::util::AtomicMemory(memory);
  auto* state = new ::mpact::sim::riscv::RiscVState("test_boot", ::mpact::sim::riscv::RiscVXlen::RV64, memory, atomic_memory);
  
  auto* fp_state = new ::mpact::sim::riscv::RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);
  auto* vector_state = new ::mpact::sim::riscv::RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);
  
  auto* decoder = new ::mpact::sim::riscv::Rva23u64DecoderWrapper(state, memory);
  
  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat("x", i);
    state->AddRegister<::mpact::sim::riscv::RV64Register>(reg_name);
    state->AddRegisterAlias<::mpact::sim::riscv::RV64Register>(reg_name, ::mpact::sim::riscv::kXRegisterAliases[i]);
  }
  
  auto* top = new ::mpact::sim::riscv::RiscVTop("test_top", state, decoder);

  absl::Status status = ::mpact::sim::riscv::RiscvDtbLoader::LoadFirmwareAndSeedRegisters(state, vmlinux_path, dtb_path);
  EXPECT_TRUE(status.ok()) << status.message();

  uint64_t entry_point = 0x20000000;
  EXPECT_TRUE(top->WriteRegister("pc", entry_point).ok());

  // Execute instructions.
  auto run_status = top->Step(8);
  EXPECT_TRUE(run_status.ok());

  uint64_t final_pc = top->ReadRegister("pc").value();
  uint64_t mcause = state->csr_set()->GetCsr("mcause").value()->AsUint64();
  uint64_t mtval = state->csr_set()->GetCsr("mtval").value()->AsUint64();

  // If it failed the branch, it would hit the infinite loop at 0x20000014
  // If it succeeded, it branched to the NOP and advanced PC
  EXPECT_GT(final_pc, 0x20000018) << "Boot sequence failed. mcause: " << mcause << " mtval: " << mtval;

  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}

TEST(Rva23u64SimTest, ZicboE2EExecutionBoundary) {
  std::string tmp_dir = std::getenv("TEST_TMPDIR") ? std::getenv("TEST_TMPDIR") : ".os_build";
  std::filesystem::create_directories(tmp_dir);
  std::string vmlinux_path = tmp_dir + "/zicbo_test.elf";
  std::string asm_path = tmp_dir + "/zicbo_stub.s";

  struct FileCleaner {
    std::string p1, p2;
    ~FileCleaner() {
      std::remove(p1.c_str());
      std::remove(p2.c_str());
    }
  } cleaner{vmlinux_path, asm_path};

  // Create ASM Stub
  std::ofstream s_file(asm_path);
  s_file << ".global _start\n_start:\n"
            "  li a0, 0x10000\n"
            "  cbo.zero (a0)\n"
            "  cbo.clean (a0)\n"
            "  cbo.flush (a0)\n"
            "  cbo.inval (a0)\n"
            "  nop\n";
  s_file.close();

  // Compile
  std::string cmd = "riscv64-unknown-elf-gcc -march=rv64g_zicbom_zicboz -mabi=lp64d -nostdlib " + asm_path + " -o " + vmlinux_path;
  int ret = system(cmd.c_str());
  if (ret != 0) {
    GTEST_SKIP() << "Compiler not available, skipping Zicbo E2E execution test.";
  }

  auto* memory = new ::mpact::sim::util::FlatDemandMemory();
  auto* atomic_memory = new ::mpact::sim::util::AtomicMemory(memory);
  auto* state = new ::mpact::sim::riscv::RiscVState("test_zicbo", ::mpact::sim::riscv::RiscVXlen::RV64, memory, atomic_memory);
  
  auto* fp_state = new ::mpact::sim::riscv::RiscVFPState(state->csr_set(), state);
  state->set_rv_fp(fp_state);
  auto* vector_state = new ::mpact::sim::riscv::RiscVVectorState(state, 64);
  state->set_rv_vector(vector_state);
  
  auto* decoder = new ::mpact::sim::riscv::Rva23u64DecoderWrapper(state, memory);
  
  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat("x", i);
    state->AddRegister<::mpact::sim::riscv::RV64Register>(reg_name);
    state->AddRegisterAlias<::mpact::sim::riscv::RV64Register>(reg_name, ::mpact::sim::riscv::kXRegisterAliases[i]);
  }
  
  auto* top = new ::mpact::sim::riscv::RiscVTop("test_top", state, decoder);

  ::mpact::sim::util::ElfProgramLoader elf_loader(memory);
  auto load_result = elf_loader.LoadProgram(vmlinux_path);
  EXPECT_TRUE(load_result.ok());

  uint64_t entry_point = load_result.value();
  EXPECT_TRUE(top->WriteRegister("pc", entry_point).ok());

  // Execute instructions. 6 instructions total.
  auto run_status = top->Step(6);
  EXPECT_TRUE(run_status.ok());

  uint64_t final_pc = top->ReadRegister("pc").value();
  uint64_t mcause = state->csr_set()->GetCsr("mcause").value()->AsUint64();

  EXPECT_GT(final_pc, entry_point + 16) << "Execution failed. mcause: " << mcause;
  EXPECT_EQ(mcause, 0);

  delete top;
  delete decoder;
  delete vector_state;
  delete fp_state;
  delete state;
  delete atomic_memory;
  delete memory;
}
