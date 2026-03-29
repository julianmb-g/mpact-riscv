// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "riscv/riscv_mmu.h"

#include "riscv/riscv_mmu.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_csr.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/generic/data_buffer.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace {

TEST(RiscVMmuTest, ConstructorRequiresPhysicalMemory) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  EXPECT_DEATH(RiscVMmu(&state, nullptr), "physical_memory must not be null");
  delete physical_memory;
}

TEST(RiscVMmuTest, ConstructorRequiresState) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  EXPECT_DEATH(RiscVMmu(nullptr, physical_memory), "state must not be null");
  delete physical_memory;
}

TEST(RiscVMmuTest, TestUnsupportedSatpModeFallback) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();

  uint64_t initial_satp = satp_csr->AsUint64();
  
  // Test multiple unsupported modes (For RV64, only 0 and 8 are valid)
  uint64_t unsupported_modes[] = {1, 2, 3, 4, 5, 6, 7, 11, 12, 13, 14, 15};
  for (uint64_t mode : unsupported_modes) {
    satp_csr->Write(static_cast<uint64_t>(mode << 60));
    EXPECT_EQ(satp_csr->AsUint64(), initial_satp) << "Mode " << mode << " write should be ignored";
  }

  delete physical_memory;
}

TEST(RiscVMmuTest, TestMmuBareModeBypass) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();
  satp_csr->Write(static_cast<uint64_t>(0)); // Bare mode

  RiscVMmu mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  auto write_db = db_factory.Allocate<uint32_t>(1);
  write_db->Set<uint32_t>(0, 0xCAFEBABE);
  mmu.Store(0x80000000, write_db);

  auto read_db = db_factory.Allocate<uint32_t>(1);
  physical_memory->Load(0x80000000, read_db, nullptr, nullptr);
  EXPECT_EQ(read_db->Get<uint32_t>(0), 0xCAFEBABE);

  write_db->DecRef();
  read_db->DecRef();
  delete physical_memory;
}

TEST(RiscVMmuTest, TestMmuReadOnlyPageStoreViolation) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();

  RiscVMmu mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  // Set satp.MODE = 8 (Sv39), PPN = 1 (Page at physical 0x1000)
  uint64_t satp_val = (8ULL << 60) | 1ULL;
  satp_csr->Write(satp_val);

  // Setup L2 PTE (vpn[2]=1) pointing to L1 at PPN 2
  auto pte2_db = db_factory.Allocate<uint64_t>(1);
  pte2_db->Set<uint64_t>(0, (2ULL << 10) | 0x1);
  physical_memory->Store(0x1008, pte2_db);

  // Setup L1 PTE (vpn[1]=0) pointing to L0 at PPN 3
  auto pte1_db = db_factory.Allocate<uint64_t>(1);
  pte1_db->Set<uint64_t>(0, (3ULL << 10) | 0x1);
  physical_memory->Store(0x2000, pte1_db);

  // Read-only page L0 PTE at PPN 4
  auto pte0_ro = db_factory.Allocate<uint64_t>(1);
  pte0_ro->Set<uint64_t>(0, (4ULL << 10) | 0xC3); // V=1, R=1, W=0, A=1, D=1
  physical_memory->Store(0x3000, pte0_ro);

  auto write_db = db_factory.Allocate<uint32_t>(1);
  write_db->Set<uint32_t>(0, 0xBADF00D);
  
  auto mcause_res = state.csr_set()->GetCsr("mcause");
  EXPECT_TRUE(mcause_res.ok());
  auto* mcause_csr = mcause_res.value();
  
  auto mtval_res = state.csr_set()->GetCsr("mtval");
  EXPECT_TRUE(mtval_res.ok());
  auto* mtval_csr = mtval_res.value();

  mcause_csr->Write(static_cast<uint64_t>(0));
  mtval_csr->Write(static_cast<uint64_t>(0));

  mmu.Store(0x40000000, write_db); // Store on read-only page
  EXPECT_EQ(mcause_csr->AsUint64(), static_cast<uint64_t>(ExceptionCode::kStorePageFault));
  EXPECT_EQ(mtval_csr->AsUint64(), 0x40000000);

  pte2_db->DecRef();
  pte1_db->DecRef();
  pte0_ro->DecRef();
  write_db->DecRef();
  delete physical_memory;
}

TEST(RiscVMmuTest, TestMmuInvalidPteTrap) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();

  RiscVMmu mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  // Set satp.MODE = 8 (Sv39), PPN = 1 (Page at physical 0x1000)
  uint64_t satp_val = (8ULL << 60) | 1ULL;
  satp_csr->Write(satp_val);

  // Setup L2 PTE (vpn[2]=1) pointing to L1 at PPN 2
  auto pte2_db = db_factory.Allocate<uint64_t>(1);
  pte2_db->Set<uint64_t>(0, (2ULL << 10) | 0x1);
  physical_memory->Store(0x1008, pte2_db);

  // Setup L1 PTE (vpn[1]=0) pointing to L0 at PPN 3
  auto pte1_db = db_factory.Allocate<uint64_t>(1);
  pte1_db->Set<uint64_t>(0, (3ULL << 10) | 0x1);
  physical_memory->Store(0x2000, pte1_db);

  // Invalid PTE V=0
  auto pte0_inv = db_factory.Allocate<uint64_t>(1);
  pte0_inv->Set<uint64_t>(0, (4ULL << 10) | 0x0); // V=0
  physical_memory->Store(0x3000, pte0_inv);

  auto write_db = db_factory.Allocate<uint32_t>(1);
  write_db->Set<uint32_t>(0, 0xBADF00D);
  
  auto mcause_res = state.csr_set()->GetCsr("mcause");
  EXPECT_TRUE(mcause_res.ok());
  auto* mcause_csr = mcause_res.value();
  
  auto mtval_res = state.csr_set()->GetCsr("mtval");
  EXPECT_TRUE(mtval_res.ok());
  auto* mtval_csr = mtval_res.value();

  mcause_csr->Write(static_cast<uint64_t>(0));
  mtval_csr->Write(static_cast<uint64_t>(0));

  mmu.Store(0x40000000, write_db); // Store on invalid page
  EXPECT_EQ(mcause_csr->AsUint64(), static_cast<uint64_t>(ExceptionCode::kStorePageFault));
  EXPECT_EQ(mtval_csr->AsUint64(), 0x40000000);

  pte2_db->DecRef();
  pte1_db->DecRef();
  pte0_inv->DecRef();
  write_db->DecRef();
  delete physical_memory;
}

TEST(RiscVMmuTest, Sv39PageWalkTranslation) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  // Get the satp CSR from the state.
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();

  // Test negative constraint: unsupported modes must be ignored.
  // 11 is unsupported.
  uint64_t initial_satp = satp_csr->AsUint64();
  satp_csr->Write(static_cast<uint64_t>(11ULL << 60));
  EXPECT_EQ(satp_csr->AsUint64(), initial_satp) << "Mode 11 write should be ignored";

  RiscVMmu mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  // Set satp.MODE = 8 (Sv39), PPN = 1 (Page at physical 0x1000)
  uint64_t satp_val = (8ULL << 60) | 1ULL;
  satp_csr->Write(satp_val);

  // Set up L2 PTE at physical 0x1000 + vpn[2]*8
  // VA: 0x40000000 -> vpn[2] = 1, vpn[1]=0, vpn[0]=0, pgoff=0
  // L2 PTE addr = 0x1000 + 1*8 = 0x1008
  // Let L2 PTE point to L1 Page Table at PPN 2 (0x2000)
  auto pte2_db = db_factory.Allocate<uint64_t>(1);
  pte2_db->Set<uint64_t>(0, (2ULL << 10) | 0x1); // V=1, non-leaf
  physical_memory->Store(0x1008, pte2_db);

  // Set up L1 PTE at physical 0x2000 + vpn[1]*8 = 0x2000
  // Let L1 PTE point to L0 Page Table at PPN 3 (0x3000)
  auto pte1_db = db_factory.Allocate<uint64_t>(1);
  pte1_db->Set<uint64_t>(0, (3ULL << 10) | 0x1); // V=1, non-leaf
  physical_memory->Store(0x2000, pte1_db);

  // Set up L0 PTE at physical 0x3000 + vpn[0]*8 = 0x3000
  // Let L0 PTE map to physical PPN 4 (0x4000). Leaf.
  // Permissions: V=1, R=1, W=1, X=0 (RW page).
  auto pte0_db = db_factory.Allocate<uint64_t>(1);
  pte0_db->Set<uint64_t>(0, (4ULL << 10) | 0xC7); // V=1, R=1, W=1, A=1, D=1
  physical_memory->Store(0x3000, pte0_db);

  // Write some data at physical address 0x4000
  auto write_db = db_factory.Allocate<uint32_t>(1);
  write_db->Set<uint32_t>(0, 0xDEADBEEF);
  physical_memory->Store(0x4000, write_db);

  // Load through MMU with VA 0x40000000
  auto read_db = db_factory.Allocate<uint32_t>(1);
  mmu.Load(0x40000000, read_db, nullptr, nullptr);
  
  EXPECT_EQ(read_db->Get<uint32_t>(0), 0xDEADBEEF);
  
  pte2_db->DecRef();
  pte1_db->DecRef();
  pte0_db->DecRef();
  write_db->DecRef();
  read_db->DecRef();
  delete physical_memory;
}

TEST(RiscVMmuTest, TestSsnpmPointerMaskingExemption) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();
  
  auto senvcfg_res = state.csr_set()->GetCsr("senvcfg");
  EXPECT_TRUE(senvcfg_res.ok());
  auto* senvcfg_csr = senvcfg_res.value();

  RiscVMmu mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  // Set satp.MODE = 8 (Sv39), PPN = 1 (Page at physical 0x1000)
  uint64_t satp_val = (8ULL << 60) | 1ULL;
  satp_csr->Write(satp_val);

  // Set senvcfg.PMM = 2 (PMLEN=54, strip top 10 bits)
  uint64_t pmm = 2ULL;
  senvcfg_csr->Write(pmm << 32);

  // Setup L2 PTE (vpn[2]=1) pointing to L1 at PPN 2
  auto pte2_db = db_factory.Allocate<uint64_t>(1);
  pte2_db->Set<uint64_t>(0, (2ULL << 10) | 0x1);
  physical_memory->Store(0x1008, pte2_db);

  // Setup L1 PTE (vpn[1]=0) pointing to L0 at PPN 3
  auto pte1_db = db_factory.Allocate<uint64_t>(1);
  pte1_db->Set<uint64_t>(0, (3ULL << 10) | 0x1);
  physical_memory->Store(0x2000, pte1_db);

  // L0 PTE at PPN 4. Valid, Readable, Writable, Executable.
  auto pte0 = db_factory.Allocate<uint64_t>(1);
  pte0->Set<uint64_t>(0, (4ULL << 10) | 0xCF); // V=1, R=1, W=1, X=1, A=1, D=1
  physical_memory->Store(0x3000, pte0);

  // Base virtual address 0x40000000 (canonical)
  // Masked virtual address: add some non-canonical tag in top 10 bits
  // e.g. set bit 60 to 1 -> 0x1000000040000000
  uint64_t tagged_vaddr = 0x1000000040000000ULL;

  auto mcause_res = state.csr_set()->GetCsr("mcause");
  EXPECT_TRUE(mcause_res.ok());
  auto* mcause_csr = mcause_res.value();
  
  auto mtval_res = state.csr_set()->GetCsr("mtval");
  EXPECT_TRUE(mtval_res.ok());
  auto* mtval_csr = mtval_res.value();

  // Test 1: Data Load MUST succeed because pointer masking is enabled and strips the tag.
  auto read_db = db_factory.Allocate<uint32_t>(1);
  mcause_csr->Write(static_cast<uint64_t>(0));
  mmu.Load(tagged_vaddr, read_db, nullptr, nullptr);
  EXPECT_EQ(mcause_csr->AsUint64(), 0) << "Data load with pointer masking should not trap";

  // Test 2: Data Store MUST succeed for the same reason.
  auto write_db = db_factory.Allocate<uint32_t>(1);
  mcause_csr->Write(static_cast<uint64_t>(0));
  mmu.Store(tagged_vaddr, write_db);
  EXPECT_EQ(mcause_csr->AsUint64(), 0) << "Data store with pointer masking should not trap";

  // Test 3: Instruction Fetch MUST trap (kInstructionPageFault) because tag stripping is FORBIDDEN on inst fetch.
  mcause_csr->Write(static_cast<uint64_t>(0));
  mtval_csr->Write(static_cast<uint64_t>(0));
  state.set_is_fetching(true);
  mmu.Load(tagged_vaddr, read_db, nullptr, nullptr);
  state.set_is_fetching(false);
  EXPECT_EQ(mcause_csr->AsUint64(), static_cast<uint64_t>(ExceptionCode::kInstructionPageFault)) 
      << "Instruction fetch with non-canonical tagged address MUST trap even if pointer masking is enabled";
  EXPECT_EQ(mtval_csr->AsUint64(), tagged_vaddr);

  pte2_db->DecRef();
  pte1_db->DecRef();
  pte0->DecRef();
  read_db->DecRef();
  write_db->DecRef();
  delete physical_memory;
}

TEST(RiscVMmuTest, Sv48PageWalkTranslation) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  RiscVMmu mmu(&state, physical_memory);

  // Set satp to mode 9 (Sv48), ASID 0, PPN 0x123.
  uint64_t mode = 9;
  uint64_t ppn = 0x123;
  uint64_t satp_val = (mode << 60) | ppn;
  auto satp_csr = state.csr_set()->GetCsr("satp").value();
  satp_csr->Write(satp_val);

  // Vaddr with 4 levels.
  uint64_t vaddr = 0x0000123456789ABCULL;
  
  // Create a valid leaf PTE at level 0 (after walking levels 3, 2, 1).
  // Root PTE (level 3)
  uint64_t a = ppn * 4096;
  uint64_t vpn_3 = (vaddr >> (12 + 27)) & 0x1FF;
  uint64_t pte3_addr = a + vpn_3 * 8;
  uint64_t pte3_ppn = 0x200;
  uint64_t pte3_val = (pte3_ppn << 10) | 0x1; // V=1, non-leaf

  // Level 2 PTE
  uint64_t vpn_2 = (vaddr >> (12 + 18)) & 0x1FF;
  uint64_t pte2_addr = pte3_ppn * 4096 + vpn_2 * 8;
  uint64_t pte2_ppn = 0x300;
  uint64_t pte2_val = (pte2_ppn << 10) | 0x1; // V=1, non-leaf

  // Level 1 PTE
  uint64_t vpn_1 = (vaddr >> (12 + 9)) & 0x1FF;
  uint64_t pte1_addr = pte2_ppn * 4096 + vpn_1 * 8;
  uint64_t pte1_ppn = 0x400;
  uint64_t pte1_val = (pte1_ppn << 10) | 0x1; // V=1, non-leaf

  // Level 0 PTE
  uint64_t vpn_0 = (vaddr >> 12) & 0x1FF;
  uint64_t pte0_addr = pte1_ppn * 4096 + vpn_0 * 8;
  uint64_t pte0_ppn = 0x500;
  uint64_t pte0_val = (pte0_ppn << 10) | 0xCF; // V=1, R=1, W=1, X=1, A=1, D=1 (leaf)

  // Write all PTEs
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  auto db = db_factory.Allocate<uint64_t>(1);

  db->Set<uint64_t>(0, pte3_val);
  physical_memory->Store(pte3_addr, db);
  
  db->Set<uint64_t>(0, pte2_val);
  physical_memory->Store(pte2_addr, db);

  db->Set<uint64_t>(0, pte1_val);
  physical_memory->Store(pte1_addr, db);

  db->Set<uint64_t>(0, pte0_val);
  physical_memory->Store(pte0_addr, db);
  
  db->DecRef();

  // Store golden data at the expected physical address
  uint64_t expected_paddr = (pte0_ppn * 4096) + (vaddr & 0xFFF);
  auto golden_db = db_factory.Allocate<uint64_t>(1);
  golden_db->Set<uint64_t>(0, 0xDEADBEEFCAFEBABELL);
  physical_memory->Store(expected_paddr, golden_db);
  golden_db->DecRef();

  // Load to trigger Translate
  auto out_db = db_factory.Allocate<uint64_t>(1);
  mmu.Load(vaddr, out_db, nullptr, nullptr);
  
  // Assert translation fetched the golden data
  EXPECT_EQ(out_db->Get<uint64_t>(0), 0xDEADBEEFCAFEBABELL) << "Failed at Sv48! expected_paddr=" << expected_paddr;

  out_db->DecRef();
  delete physical_memory;
}

TEST(RiscVMmuTest, Sv57PageWalkTranslation) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  RiscVMmu mmu(&state, physical_memory);

  uint64_t mode = 10; // Sv57
  uint64_t ppn = 0x123;
  uint64_t satp_val = (mode << 60) | ppn;
  auto satp_csr = state.csr_set()->GetCsr("satp").value();
  satp_csr->Write(satp_val);

  uint64_t vaddr = 0x00123456789ABCDEULL; // 57 bits canonical
  
  uint64_t a = ppn * 4096;
  uint64_t vpn_4 = (vaddr >> (12 + 36)) & 0x1FF;
  uint64_t pte4_addr = a + vpn_4 * 8;
  uint64_t pte4_ppn = 0x200;
  uint64_t pte4_val = (pte4_ppn << 10) | 0x1;

  uint64_t vpn_3 = (vaddr >> (12 + 27)) & 0x1FF;
  uint64_t pte3_addr = pte4_ppn * 4096 + vpn_3 * 8;
  uint64_t pte3_ppn = 0x300;
  uint64_t pte3_val = (pte3_ppn << 10) | 0x1;

  uint64_t vpn_2 = (vaddr >> (12 + 18)) & 0x1FF;
  uint64_t pte2_addr = pte3_ppn * 4096 + vpn_2 * 8;
  uint64_t pte2_ppn = 0x400;
  uint64_t pte2_val = (pte2_ppn << 10) | 0x1;

  uint64_t vpn_1 = (vaddr >> (12 + 9)) & 0x1FF;
  uint64_t pte1_addr = pte2_ppn * 4096 + vpn_1 * 8;
  uint64_t pte1_ppn = 0x500;
  uint64_t pte1_val = (pte1_ppn << 10) | 0x1;

  uint64_t vpn_0 = (vaddr >> 12) & 0x1FF;
  uint64_t pte0_addr = pte1_ppn * 4096 + vpn_0 * 8;
  uint64_t pte0_ppn = 0x600;
  uint64_t pte0_val = (pte0_ppn << 10) | 0xCF;

  auto db_factory = mpact::sim::generic::DataBufferFactory();
  auto db = db_factory.Allocate<uint64_t>(1);
  db->Set<uint64_t>(0, pte4_val); physical_memory->Store(pte4_addr, db);
  db->Set<uint64_t>(0, pte3_val); physical_memory->Store(pte3_addr, db);
  db->Set<uint64_t>(0, pte2_val); physical_memory->Store(pte2_addr, db);
  db->Set<uint64_t>(0, pte1_val); physical_memory->Store(pte1_addr, db);
  db->Set<uint64_t>(0, pte0_val); physical_memory->Store(pte0_addr, db);
  db->DecRef();

  // Store golden data at the expected physical address
  uint64_t expected_paddr = (pte0_ppn * 4096) + (vaddr & 0xFFF);
  auto golden_db = db_factory.Allocate<uint64_t>(1);
  golden_db->Set<uint64_t>(0, 0x1234567890ABCDEFULL);
  physical_memory->Store(expected_paddr, golden_db);
  golden_db->DecRef();

  auto out_db = db_factory.Allocate<uint64_t>(1);
  mmu.Load(vaddr, out_db, nullptr, nullptr);
  
  EXPECT_EQ(out_db->Get<uint64_t>(0), 0x1234567890ABCDEFULL);

  out_db->DecRef();
  delete physical_memory;
}

TEST(RiscVMmuTest, TestSv39MmuPageFault) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  ASSERT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();

  RiscVMmu mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  // Set satp.MODE = 8 (Sv39), PPN = 1 (Page at physical 0x1000)
  uint64_t satp_val = (8ULL << 60) | 1ULL;
  satp_csr->Write(satp_val);

  // Setup L2 PTE (vpn[2]=1) pointing to L1 at PPN 2
  auto pte2_db = db_factory.Allocate<uint64_t>(1);
  pte2_db->Set<uint64_t>(0, (2ULL << 10) | 0x1);
  physical_memory->Store(0x1008, pte2_db);

  // Setup L1 PTE (vpn[1]=0) pointing to L0 at PPN 3
  auto pte1_db = db_factory.Allocate<uint64_t>(1);
  pte1_db->Set<uint64_t>(0, (3ULL << 10) | 0x1);
  physical_memory->Store(0x2000, pte1_db);

  // L0 PTE at PPN 4. Valid, Readable, Writable, NOT Executable.
  auto pte0_nx = db_factory.Allocate<uint64_t>(1);
  pte0_nx->Set<uint64_t>(0, (4ULL << 10) | 0xC3); // V=1, R=1, W=1, X=0, A=1, D=1
  physical_memory->Store(0x3000, pte0_nx);

  auto read_db = db_factory.Allocate<uint32_t>(1);
  
  auto mcause_res = state.csr_set()->GetCsr("mcause");
  ASSERT_TRUE(mcause_res.ok());
  auto* mcause_csr = mcause_res.value();
  
  auto mtval_res = state.csr_set()->GetCsr("mtval");
  ASSERT_TRUE(mtval_res.ok());
  auto* mtval_csr = mtval_res.value();

  mcause_csr->Write(static_cast<uint64_t>(0));
  mtval_csr->Write(static_cast<uint64_t>(0));

  state.set_is_fetching(true);
  mmu.Load(0x40000000, read_db, nullptr, nullptr); // Fetch on NX page
  state.set_is_fetching(false);

  EXPECT_EQ(mcause_csr->AsUint64(), static_cast<uint64_t>(ExceptionCode::kInstructionPageFault));
  EXPECT_EQ(mtval_csr->AsUint64(), 0x40000000);

  pte2_db->DecRef();
  pte1_db->DecRef();
  pte0_nx->DecRef();
  read_db->DecRef();
  delete physical_memory;
}


class RiscVMmuSv48TranslationTest : public ::testing::Test {
 protected:
  RiscVMmuSv48TranslationTest() {
    physical_memory = new mpact::sim::util::FlatDemandMemory();
    state = new RiscVState("test_sv48", RiscVXlen::RV64, physical_memory);
    mmu = new RiscVMmu(state, physical_memory);
  }

  ~RiscVMmuSv48TranslationTest() override {
    delete mmu;
    delete state;
    delete physical_memory;
  }

  mpact::sim::util::FlatDemandMemory* physical_memory;
  RiscVState* state;
  RiscVMmu* mmu;
};

TEST_F(RiscVMmuSv48TranslationTest, Sv48PageWalkTranslationOrganic) {
  uint64_t mode = 9; // Sv48
  uint64_t ppn = 0x123;
  uint64_t satp_val = (mode << 60) | ppn;
  auto satp_csr = state->csr_set()->GetCsr("satp").value();
  satp_csr->Write(satp_val);

  uint64_t vaddr = 0x0000123456789ABCULL;
  
  // Set up dummy page tables
  uint64_t a = ppn * 4096;
  uint64_t vpn_3 = (vaddr >> 39) & 0x1FF;
  uint64_t pte3_addr = a + vpn_3 * 8;
  uint64_t pte3_ppn = 0x200;
  uint64_t pte3_val = (pte3_ppn << 10) | 0x1; // Valid, points to next level
  
  auto* db_factory = state->db_factory();
  auto* write_db = db_factory->Allocate<uint64_t>(1);
  write_db->Set<uint64_t>(0, pte3_val);
  physical_memory->Store(pte3_addr, write_db);
  write_db->DecRef();

  // Next level (level 2)
  uint64_t pte2_addr = pte3_ppn * 4096 + ((vaddr >> 30) & 0x1FF) * 8;
  uint64_t pte2_ppn = 0x300;
  uint64_t pte2_val = (pte2_ppn << 10) | 0x1;
  write_db = db_factory->Allocate<uint64_t>(1);
  write_db->Set<uint64_t>(0, pte2_val);
  physical_memory->Store(pte2_addr, write_db);
  write_db->DecRef();

  // Next level (level 1)
  uint64_t pte1_addr = pte2_ppn * 4096 + ((vaddr >> 21) & 0x1FF) * 8;
  uint64_t pte1_ppn = 0x400;
  uint64_t pte1_val = (pte1_ppn << 10) | 0x1;
  write_db = db_factory->Allocate<uint64_t>(1);
  write_db->Set<uint64_t>(0, pte1_val);
  physical_memory->Store(pte1_addr, write_db);
  write_db->DecRef();

  // Leaf level (level 0)
  uint64_t pte0_addr = pte1_ppn * 4096 + ((vaddr >> 12) & 0x1FF) * 8;
  uint64_t pte0_ppn = 0x500;
  uint64_t pte0_val = (pte0_ppn << 10) | 0xCF; // Valid + R/W/X + A/D
  write_db = db_factory->Allocate<uint64_t>(1);
  write_db->Set<uint64_t>(0, pte0_val);
  physical_memory->Store(pte0_addr, write_db);
  write_db->DecRef();

  uint64_t expected_paddr = (pte0_ppn * 4096) | (vaddr & 0xFFF);
  auto golden_db = db_factory->Allocate<uint64_t>(1);
  golden_db->Set<uint64_t>(0, 0xDEADBEEFCAFEBABELL);
  physical_memory->Store(expected_paddr, golden_db);
  golden_db->DecRef();

  // Load to trigger Translate
  auto out_db = db_factory->Allocate<uint64_t>(1);
  mmu->Load(vaddr, out_db, nullptr, nullptr);
  
  // Assert translation fetched the golden data
  EXPECT_EQ(out_db->Get<uint64_t>(0), 0xDEADBEEFCAFEBABELL) << "Failed at Sv48! expected_paddr=" << expected_paddr;
  out_db->DecRef();
}

TEST(RiscVMmuTest, TestSvpbmtReservedFault) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  state.AddExtension("Svpbmt");
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();

  RiscVMmu mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  uint64_t satp_val = (8ULL << 60) | 1ULL; // Sv39, PPN=1
  satp_csr->Write(satp_val);

  // L2 PTE
  auto pte2_db = db_factory.Allocate<uint64_t>(1);
  pte2_db->Set<uint64_t>(0, (2ULL << 10) | 0x1);
  physical_memory->Store(0x1008, pte2_db);

  // L1 PTE
  auto pte1_db = db_factory.Allocate<uint64_t>(1);
  pte1_db->Set<uint64_t>(0, (3ULL << 10) | 0x1);
  physical_memory->Store(0x2000, pte1_db);

  // L0 PTE (Leaf) with PBMT = 11 (Reserved)
  auto pte0_pbmt = db_factory.Allocate<uint64_t>(1);
  uint64_t pte_val = (4ULL << 10) | 0xCF; // V=1, R/W/X=1, A=1, D=1
  pte_val |= (3ULL << 61); // PBMT = 11 (Reserved)
  pte0_pbmt->Set<uint64_t>(0, pte_val);
  physical_memory->Store(0x3000, pte0_pbmt);

  auto read_db = db_factory.Allocate<uint32_t>(1);
  
  auto mcause_res = state.csr_set()->GetCsr("mcause");
  EXPECT_TRUE(mcause_res.ok());
  auto* mcause_csr = mcause_res.value();
  mcause_csr->Write(static_cast<uint64_t>(0));

  mmu.Load(0x40000000, read_db, nullptr, nullptr); 
  EXPECT_EQ(mcause_csr->AsUint64(), static_cast<uint64_t>(ExceptionCode::kLoadPageFault)) 
      << "Reserved PBMT=11 MUST cause a Page Fault";

  pte2_db->DecRef();
  pte1_db->DecRef();
  pte0_pbmt->DecRef();
  read_db->DecRef();
  delete physical_memory;
}

TEST(RiscVMmuTest, TestSvnapotTranslation) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  state.AddExtension("Svnapot");
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();

  RiscVMmu mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  uint64_t satp_val = (8ULL << 60) | 1ULL; // Sv39, PPN=1
  satp_csr->Write(satp_val);

  // L2 PTE
  auto pte2_db = db_factory.Allocate<uint64_t>(1);
  pte2_db->Set<uint64_t>(0, (2ULL << 10) | 0x1);
  physical_memory->Store(0x1008, pte2_db);

  // L1 PTE
  auto pte1_db = db_factory.Allocate<uint64_t>(1);
  pte1_db->Set<uint64_t>(0, (3ULL << 10) | 0x1);
  physical_memory->Store(0x2000, pte1_db);

  // L0 PTE (Leaf) with N = 1 (NAPOT 64KB page)
  auto pte0_napot = db_factory.Allocate<uint64_t>(1);
  uint64_t pte_ppn = (4ULL << 4) | 0x8; // PPN[3:0] MUST be 1000 (8) for 64KB NAPOT
  uint64_t pte_val = (pte_ppn << 10) | 0xCF; // V=1, R/W/X=1, A=1, D=1
  pte_val |= (1ULL << 63); // N = 1 (NAPOT)
  pte0_napot->Set<uint64_t>(0, pte_val);
  physical_memory->Store(0x3008, pte0_napot);

  // Write golden data to physical address.
  // VA = 0x40001234. Since it's a 64KB page, the offset is 16 bits: 0x1234.
  // Physical address = ((pte_ppn & ~0xF) * 4096) + 0x1234
  // pte_ppn & ~0xF = (4 << 4) = 0x40. 0x40 * 4096 = 0x40000.
  // Paddr = 0x40000 + 0x1234 = 0x41234.
  auto write_db = db_factory.Allocate<uint32_t>(1);
  write_db->Set<uint32_t>(0, 0x0B00B1E5);
  physical_memory->Store(0x41234, write_db);

  auto read_db = db_factory.Allocate<uint32_t>(1);
  mmu.Load(0x40001234, read_db, nullptr, nullptr); 
  
  auto mcause_res = state.csr_set()->GetCsr("mcause");
  EXPECT_TRUE(mcause_res.ok());
  auto* mcause_csr = mcause_res.value();
  
  EXPECT_EQ(mcause_csr->AsUint64(), 0) << "NAPOT translation should succeed";
  EXPECT_EQ(read_db->Get<uint32_t>(0), 0x0B00B1E5);

  pte2_db->DecRef();
  pte1_db->DecRef();
  pte0_napot->DecRef();
  write_db->DecRef();
  read_db->DecRef();
  delete physical_memory;
}


TEST(RiscVMmuTest, TestSvaduPageFault) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  // Do NOT add Svadu
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  auto* satp_csr = satp_res.value();

  RiscVMmu mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  satp_csr->Write(static_cast<uint64_t>((8ULL << 60) | 1ULL));

  auto pte2_db = db_factory.Allocate<uint64_t>(1);
  pte2_db->Set<uint64_t>(0, (2ULL << 10) | 0x1);
  physical_memory->Store(0x1008, pte2_db);

  auto pte1_db = db_factory.Allocate<uint64_t>(1);
  pte1_db->Set<uint64_t>(0, (3ULL << 10) | 0x1);
  physical_memory->Store(0x2000, pte1_db);

  // V=1, R=1, W=1, X=0, but A=0, D=0
  auto pte0 = db_factory.Allocate<uint64_t>(1);
  pte0->Set<uint64_t>(0, (4ULL << 10) | 0x7); 
  physical_memory->Store(0x3000, pte0);

  auto read_db = db_factory.Allocate<uint32_t>(1);
  
  auto mcause_csr = state.csr_set()->GetCsr("mcause").value();
  mcause_csr->Write(static_cast<uint64_t>(0));

  mmu.Load(0x40000000, read_db, nullptr, nullptr); 

  EXPECT_EQ(mcause_csr->AsUint64(), static_cast<uint64_t>(ExceptionCode::kLoadPageFault));

  pte2_db->DecRef();
  pte1_db->DecRef();
  pte0->DecRef();
  read_db->DecRef();
  delete physical_memory;
}

TEST(RiscVMmuTest, TestSvaduHardwareUpdate) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  state.AddExtension("Svadu");
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  auto* satp_csr = satp_res.value();

  auto menvcfg_res = state.csr_set()->GetCsr("menvcfg");
  auto* menvcfg_csr = menvcfg_res.value();
  // Enable ADUE in menvcfg (bit 61)
  menvcfg_csr->Write(static_cast<uint64_t>(1ULL << 61));

  RiscVMmu mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  satp_csr->Write(static_cast<uint64_t>((8ULL << 60) | 1ULL));

  auto pte2_db = db_factory.Allocate<uint64_t>(1);
  pte2_db->Set<uint64_t>(0, (2ULL << 10) | 0x1);
  physical_memory->Store(0x1008, pte2_db);

  auto pte1_db = db_factory.Allocate<uint64_t>(1);
  pte1_db->Set<uint64_t>(0, (3ULL << 10) | 0x1);
  physical_memory->Store(0x2000, pte1_db);

  // V=1, R=1, W=1, X=0, A=0, D=0 (Missing A and D)
  auto pte0 = db_factory.Allocate<uint64_t>(1);
  pte0->Set<uint64_t>(0, (4ULL << 10) | 0x7); 
  physical_memory->Store(0x3000, pte0);

  // Target physical page payload
  auto write_db = db_factory.Allocate<uint32_t>(1);
  write_db->Set<uint32_t>(0, 0x12345678);
  physical_memory->Store(0x4000, write_db);

  auto read_db = db_factory.Allocate<uint32_t>(1);
  
  auto mcause_csr = state.csr_set()->GetCsr("mcause").value();
  mcause_csr->Write(static_cast<uint64_t>(0));

  // Perform Store to trigger A and D updates
  mmu.Store(0x40000000, read_db); 

  EXPECT_EQ(mcause_csr->AsUint64(), 0) << "Hardware update must prevent Page Fault";

  auto pte0_read = db_factory.Allocate<uint64_t>(1);
  physical_memory->Load(0x3000, pte0_read, nullptr, nullptr);
  
  uint64_t pte_updated = pte0_read->Get<uint64_t>(0);
  EXPECT_TRUE((pte_updated & (1ULL << 6)) != 0) << "A bit was not updated";
  EXPECT_TRUE((pte_updated & (1ULL << 7)) != 0) << "D bit was not updated";

  pte2_db->DecRef();
  pte1_db->DecRef();
  pte0->DecRef();
  read_db->DecRef();
  write_db->DecRef();
  pte0_read->DecRef();
  delete physical_memory;
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact

