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

#include "riscv/mmu_sv39.h"

#include "riscv/mmu_sv39.h"
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

TEST(MmuSv39Test, ConstructorRequiresPhysicalMemory) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  EXPECT_DEATH(MmuSv39(&state, nullptr), "physical_memory must not be null");
  delete physical_memory;
}

TEST(MmuSv39Test, ConstructorRequiresState) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  EXPECT_DEATH(MmuSv39(nullptr, physical_memory), "state must not be null");
  delete physical_memory;
}

TEST(MmuSv39Test, TestUnsupportedSatpModeFallback) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();

  uint64_t initial_satp = satp_csr->AsUint64();
  
  satp_csr->Write(static_cast<uint64_t>(9ULL << 60)); // Sv48
  EXPECT_EQ(satp_csr->AsUint64(), initial_satp) << "Sv48 mode write should be ignored";
  
  satp_csr->Write(static_cast<uint64_t>(10ULL << 60)); // Sv57
  EXPECT_EQ(satp_csr->AsUint64(), initial_satp) << "Sv57 mode write should be ignored";

  delete physical_memory;
}

TEST(MmuSv39Test, BareModeTranslation) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();
  satp_csr->Write(static_cast<uint64_t>(0)); // Bare mode

  MmuSv39 mmu(&state, physical_memory);
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  
  auto write_db = db_factory.Allocate<uint32_t>(1);
  write_db->Set<uint32_t>(0, 0xCAFEBABE);
  mmu.Store(0x80000000, write_db);

  auto read_db = db_factory.Allocate<uint32_t>(1);
  mmu.Load(0x80000000, read_db, nullptr, nullptr);
  EXPECT_EQ(read_db->Get<uint32_t>(0), 0xCAFEBABE);

  write_db->DecRef();
  read_db->DecRef();
  delete physical_memory;
}

TEST(MmuSv39Test, NegativePteFaults) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();

  MmuSv39 mmu(&state, physical_memory);
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
  pte0_ro->Set<uint64_t>(0, (4ULL << 10) | 0x3); // V=1, R=1, W=0
  physical_memory->Store(0x3000, pte0_ro);

  auto write_db = db_factory.Allocate<uint32_t>(1);
  write_db->Set<uint32_t>(0, 0xBADF00D);
  
  bool trapped = false;
  state.set_on_trap([&trapped](bool, uint64_t, uint64_t, uint64_t, const mpact::sim::generic::Instruction*) -> bool {
    trapped = true;
    return true;
  });

  trapped = false;
  mmu.Store(0x40000000, write_db); // Store on read-only page
  EXPECT_TRUE(trapped); // Should trigger trap

  // Invalid PTE V=0
  auto pte0_inv = db_factory.Allocate<uint64_t>(1);
  pte0_inv->Set<uint64_t>(0, (4ULL << 10) | 0x0); // V=0
  physical_memory->Store(0x3000, pte0_inv);

  trapped = false;
  mmu.Store(0x40000000, write_db); // Store on invalid page
  EXPECT_TRUE(trapped); // Should trigger trap

  pte2_db->DecRef();
  pte1_db->DecRef();
  pte0_ro->DecRef();
  pte0_inv->DecRef();
  write_db->DecRef();
  delete physical_memory;
}

TEST(MmuSv39Test, Sv39PageWalkTranslation) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  RiscVState state("test", RiscVXlen::RV64, physical_memory);
  
  // Get the satp CSR from the state.
  auto satp_res = state.csr_set()->GetCsr("satp");
  EXPECT_TRUE(satp_res.ok());
  auto* satp_csr = satp_res.value();

  // Test negative constraint: unsupported modes must be ignored.
  // 9 is Sv48, 10 is Sv57. Both should be ignored.
  uint64_t initial_satp = satp_csr->AsUint64();
  satp_csr->Write(static_cast<uint64_t>(9ULL << 60));
  EXPECT_EQ(satp_csr->AsUint64(), initial_satp) << "Sv48 mode write should be ignored";
  satp_csr->Write(static_cast<uint64_t>(10ULL << 60));
  EXPECT_EQ(satp_csr->AsUint64(), initial_satp) << "Sv57 mode write should be ignored";

  MmuSv39 mmu(&state, physical_memory);
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
  pte0_db->Set<uint64_t>(0, (4ULL << 10) | 0x7); // V=1, R=1, W=1
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

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
