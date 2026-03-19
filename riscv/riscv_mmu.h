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

#ifndef MPACT_RISCV_RISCV_RISCV_MMU_H_
#define MPACT_RISCV_RISCV_RISCV_MMU_H_

#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "riscv/riscv_state.h"

namespace mpact {
namespace sim {
namespace riscv {

// The RiscVMmu class scaffolds the RISC-V Sv39, Sv48, and Sv57 Virtual Memory architectures.
// It will implement page walking and address translation logic according
// to the RISC-V Privileged Architecture specifications.
class RiscVMmu : public mpact::sim::util::MemoryInterface {
 public:
  explicit RiscVMmu(RiscVState* state, mpact::sim::util::MemoryInterface* physical_memory);
  ~RiscVMmu() override;

  // MemoryInterface methods.
  void Load(uint64_t address, mpact::sim::generic::DataBuffer* db,
            mpact::sim::generic::Instruction* inst,
            mpact::sim::generic::ReferenceCount* context) override;
            
  void Load(mpact::sim::generic::DataBuffer* address_db,
            mpact::sim::generic::DataBuffer* mask_db, int el_size,
            mpact::sim::generic::DataBuffer* db,
            mpact::sim::generic::Instruction* inst,
            mpact::sim::generic::ReferenceCount* context) override;
            
  void Store(uint64_t address, mpact::sim::generic::DataBuffer* db) override;
  
  void Store(mpact::sim::generic::DataBuffer* address_db,
             mpact::sim::generic::DataBuffer* mask_db, int el_size,
             mpact::sim::generic::DataBuffer* db) override;

 private:
  bool Translate(uint64_t vaddr, bool is_store, bool is_inst_fetch, uint64_t* paddr);

  RiscVState* state_;
  mpact::sim::util::MemoryInterface* physical_memory_;
};

}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_RISCV_RISCV_MMU_H_
