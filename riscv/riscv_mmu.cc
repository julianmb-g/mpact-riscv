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

#include "absl/log/log.h"
#include "absl/log/check.h"
#include "riscv/riscv_mmu.h"
#include "riscv/riscv_state.h"

namespace mpact {
namespace sim {
namespace riscv {

RiscVMmu::RiscVMmu(RiscVState* state, mpact::sim::util::MemoryInterface* physical_memory)
    : state_(state), physical_memory_(physical_memory) {
  CHECK(physical_memory_ != nullptr) << "physical_memory must not be null";
  CHECK(state_ != nullptr) << "state must not be null";
}

RiscVMmu::~RiscVMmu() {}

bool RiscVMmu::Translate(uint64_t vaddr, bool is_store, bool is_inst_fetch, uint64_t* paddr) {
  auto satp_csr_status = state_->csr_set()->GetCsr("satp");
  CHECK(satp_csr_status.ok() && satp_csr_status.value() != nullptr) 
      << "satp CSR not found in RiscVState";
      
  uint64_t satp = satp_csr_status.value()->AsUint64();

  uint64_t mode = (satp >> 60) & 0xF;
  if (mode == 0) { // Bare mode
    *paddr = vaddr;
    return true;
  }
  
  int levels = 0;
  if (mode == 8) levels = 3;       // Sv39
  else if (mode == 9) levels = 4;  // Sv48
  else if (mode == 10) levels = 5; // Sv57
  else return false;

  // Pointer Masking logic (Ssnpm)
  if (!is_inst_fetch) {
    auto senvcfg_status = state_->csr_set()->GetCsr("senvcfg");
    if (senvcfg_status.ok() && senvcfg_status.value() != nullptr) {
      uint64_t senvcfg = senvcfg_status.value()->AsUint64();
      uint64_t pmm = (senvcfg >> 32) & 0x3;
      if (pmm == 2) { // PMLEN=54 (strip top 10 bits: 63:54)
        uint64_t sign_bit = (vaddr >> 53) & 1;
        uint64_t extension = sign_bit ? 0xFFC0000000000000ULL : 0;
        vaddr = (vaddr & 0x003FFFFFFFFFFFFFULL) | extension;
      } else if (pmm == 3) { // PMLEN=48 (strip top 16 bits: 63:48)
        uint64_t sign_bit = (vaddr >> 47) & 1;
        uint64_t extension = sign_bit ? 0xFFFF000000000000ULL : 0;
        vaddr = (vaddr & 0x0000FFFFFFFFFFFFULL) | extension;
      }
    }
  }

  // Canonical address check
  int vaddr_bits = 12 + 9 * levels;
  uint64_t sign_bit = (vaddr >> (vaddr_bits - 1)) & 1;
  uint64_t upper_bits = vaddr >> vaddr_bits;
  uint64_t expected_upper = sign_bit ? ((1ULL << (64 - vaddr_bits)) - 1) : 0;
  if (upper_bits != expected_upper) {
    return false; // Instruction/Load/Store Page Fault
  }

  uint64_t ppn = satp & 0xFFFFFFFFFFF;
  uint64_t a = ppn * 4096;
  int i = levels - 1;
  
  while (i >= 0) {
    uint64_t vpn_i = (vaddr >> (12 + 9 * i)) & 0x1FF;
    uint64_t pte_addr = a + vpn_i * 8;
    
    // Read PTE from physical memory.
    auto db_factory = mpact::sim::generic::DataBufferFactory();
    auto pte_db = db_factory.Allocate<uint64_t>(1);
    physical_memory_->Load(pte_addr, pte_db, nullptr, nullptr);
    uint64_t pte = pte_db->Get<uint64_t>(0);
    pte_db->DecRef();
    
    uint64_t v = pte & 0x1;
    uint64_t r = (pte >> 1) & 0x1;
    uint64_t w = (pte >> 2) & 0x1;
    uint64_t x = (pte >> 3) & 0x1;
    
    if (v == 0 || (r == 0 && w == 1)) {
      return false; // Page fault
    }
    
    if (r == 1 || x == 1) {
      if (is_store && w == 0) return false; // Fault on write to read/exec only page
      if (is_inst_fetch && x == 0) return false; // Fault on execute from non-exec page
      
      // Leaf PTE found
      uint64_t pte_ppn = (pte >> 10) & 0xFFFFFFFFFFF;
      if (i > 0) {
        // Superpage alignment check
        uint64_t ppn_mask = (1ULL << (9 * i)) - 1;
        if ((pte_ppn & ppn_mask) != 0) return false;
        
        // Form physical address
        uint64_t pgoff = vaddr & ((1ULL << (12 + 9 * i)) - 1);
        *paddr = ((pte_ppn & ~ppn_mask) * 4096) + pgoff;
      } else {
        *paddr = (pte_ppn * 4096) + (vaddr & 0xFFF);
      }
      return true;
    } else {
      if (i == 0) return false; // Fault on leaf non-terminal
      uint64_t pte_ppn = (pte >> 10) & 0xFFFFFFFFFFF;
      a = pte_ppn * 4096;
      i = i - 1;
    }
  }
  return false;
}

void RiscVMmu::Load(uint64_t address, mpact::sim::generic::DataBuffer* db,
                   mpact::sim::generic::Instruction* inst,
                   mpact::sim::generic::ReferenceCount* context) {
  uint64_t paddr = 0;
  bool is_fetch = state_->is_fetching();
  if (Translate(address, /*is_store=*/false, is_fetch, &paddr)) {
    physical_memory_->Load(paddr, db, inst, context);
  } else {
    uint64_t epc = inst != nullptr ? inst->address() : 0;
    ExceptionCode fault = is_fetch ? ExceptionCode::kInstructionPageFault : ExceptionCode::kLoadPageFault;
    state_->Trap(/*is_interrupt=*/false, address, static_cast<uint64_t>(fault), epc, inst);
  }
}

void RiscVMmu::Load(mpact::sim::generic::DataBuffer* address_db,
                   mpact::sim::generic::DataBuffer* mask_db, int el_size,
                   mpact::sim::generic::DataBuffer* db,
                   mpact::sim::generic::Instruction* inst,
                   mpact::sim::generic::ReferenceCount* context) {
  LOG(FATAL) << "Vectorized loads not supported by RiscVMmu yet";
}

void RiscVMmu::Store(uint64_t address, mpact::sim::generic::DataBuffer* db) {
  uint64_t paddr = 0;
  if (Translate(address, /*is_store=*/true, /*is_inst_fetch=*/false, &paddr)) {
    physical_memory_->Store(paddr, db);
  } else {
    // Determine instruction context implicitly if needed, or trap contextless.
    state_->Trap(/*is_interrupt=*/false, address, static_cast<uint64_t>(ExceptionCode::kStorePageFault), 0, nullptr);
  }
}

void RiscVMmu::Store(mpact::sim::generic::DataBuffer* address_db,
                    mpact::sim::generic::DataBuffer* mask_db, int el_size,
                    mpact::sim::generic::DataBuffer* db) {
  LOG(FATAL) << "Vectorized stores not supported by RiscVMmu yet";
}

}  // namespace riscv
}  // namespace sim
}  // namespace mpact
