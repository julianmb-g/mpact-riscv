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
#include "absl/log/check.h"

namespace mpact {
namespace sim {
namespace riscv {

MmuSv39::MmuSv39(mpact::sim::util::MemoryInterface* physical_memory)
    : physical_memory_(physical_memory) {
  CHECK(physical_memory_ != nullptr) << "physical_memory must not be null";
}

MmuSv39::~MmuSv39() {}

void MmuSv39::Load(uint64_t address, mpact::sim::generic::DataBuffer* db,
                   mpact::sim::generic::Instruction* inst,
                   mpact::sim::generic::ReferenceCount* context) {
  // TODO: Implement Sv39 translation, then load from physical_memory_.
  physical_memory_->Load(address, db, inst, context);
}

void MmuSv39::Load(mpact::sim::generic::DataBuffer* address_db,
                   mpact::sim::generic::DataBuffer* mask_db, int el_size,
                   mpact::sim::generic::DataBuffer* db,
                   mpact::sim::generic::Instruction* inst,
                   mpact::sim::generic::ReferenceCount* context) {
  // TODO: Implement vectorized/batched Sv39 translation.
  physical_memory_->Load(address_db, mask_db, el_size, db, inst, context);
}

void MmuSv39::Store(uint64_t address, mpact::sim::generic::DataBuffer* db) {
  // TODO: Implement Sv39 translation, then store to physical_memory_.
  physical_memory_->Store(address, db);
}

void MmuSv39::Store(mpact::sim::generic::DataBuffer* address_db,
                    mpact::sim::generic::DataBuffer* mask_db, int el_size,
                    mpact::sim::generic::DataBuffer* db) {
  // TODO: Implement vectorized/batched Sv39 translation.
  physical_memory_->Store(address_db, mask_db, el_size, db);
}

}  // namespace riscv
}  // namespace sim
}  // namespace mpact
