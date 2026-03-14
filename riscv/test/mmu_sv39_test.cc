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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/generic/data_buffer.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace {

TEST(MmuSv39Test, ConstructorRequiresPhysicalMemory) {
  EXPECT_DEATH(MmuSv39(nullptr), "physical_memory must not be null");
}

TEST(MmuSv39Test, ForwardsLoadsAndStores) {
  auto* physical_memory = new mpact::sim::util::FlatDemandMemory();
  MmuSv39 mmu(physical_memory);
  
  auto db_factory = mpact::sim::generic::DataBufferFactory();
  auto write_db = db_factory.Allocate<uint32_t>(1);
  write_db->Set<uint32_t>(0, 0x12345678);
  
  // Store through MMU
  mmu.Store(0x1000, write_db);
  
  // Load through MMU
  auto read_db = db_factory.Allocate<uint32_t>(1);
  mmu.Load(0x1000, read_db, nullptr, nullptr);
  
  EXPECT_EQ(read_db->Get<uint32_t>(0), 0x12345678);
  
  write_db->DecRef();
  read_db->DecRef();
  delete physical_memory;
}

}  // namespace
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
