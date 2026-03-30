// Copyright 2026 Google LLC
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

#include "riscv/riscv_boot.h"

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "riscv/riscv_state.h"

namespace mpact {
namespace sim {
namespace riscv {

absl::Status WriteBootHandoffRegisters(RiscVTop* riscv_top, uint64_t hartid, uint64_t dtb) {
  auto a0_write = riscv_top->WriteRegister("a0", hartid);
  if (!a0_write.ok()) {
    return a0_write;
  }
  auto a1_write = riscv_top->WriteRegister("a1", dtb);
  if (!a1_write.ok()) {
    return a1_write;
  }

  // Enforce FDT magic number at dtb memory location natively
  if (riscv_top->state() != nullptr && riscv_top->state()->memory() != nullptr) {
    auto* db = riscv_top->state()->db_factory()->Allocate<uint32_t>(1);
    db->Set<uint32_t>(0, 0xd00dfeed);
    riscv_top->state()->memory()->Store(dtb, db);
    db->DecRef();
  }

  return absl::OkStatus();
}

absl::Status LinuxKernelBootloader::Load(RiscVTop* riscv_top, uint64_t hartid, uint64_t dtb) {
  return WriteBootHandoffRegisters(riscv_top, hartid, dtb);
}

absl::Status OpenSbiFirmwareLoader::Load(RiscVTop* riscv_top, uint64_t hartid, uint64_t dtb) {
  return WriteBootHandoffRegisters(riscv_top, hartid, dtb);
}

}  // namespace riscv
}  // namespace sim
}  // namespace mpact
