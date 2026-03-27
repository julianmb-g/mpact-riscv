// Copyright 2023 Google LLC
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

#include "riscv/riscv_sim_csrs.h"

#include "riscv/riscv_state.h"
namespace mpact {
namespace sim {
namespace riscv {

MseccfgCsr::MseccfgCsr(RiscVState *state)
    : RiscVSimpleCsr<uint64_t>("mseccfg", 0x747, 0, 0x7, 0x7, state) {}

void MseccfgCsr::Write(uint64_t value) {
  uint64_t current = GetUint64();
  uint64_t next_val = current;
  // MML (bit 0) is sticky.
  if (value & 1) next_val |= 1;
  // MMWP (bit 1) is sticky.
  if (value & 2) next_val |= 2;
  // RLB (bit 2) is writable only if MML is 0.
  if (!(current & 1)) {
    next_val = (next_val & ~4ULL) | (value & 4);
  }
  Set(next_val);
}

void MseccfgCsr::Write(uint32_t value) {
  Write(static_cast<uint64_t>(value));
}

STimeCmpCsr::STimeCmpCsr(RiscVState *state,
                         std::function<void(uint64_t)> timer_cb)
    : RiscVSimpleCsr<uint64_t>("stimecmp", 0x14D, 0ULL, -1ULL, -1ULL, state),
      timer_cb_(std::move(timer_cb)) {}

void STimeCmpCsr::Write(uint64_t value) {
  Set(value);
  if (timer_cb_) {
    timer_cb_(value);
  }
}

void STimeCmpCsr::Write(uint32_t value) {
  Write(static_cast<uint64_t>(value));
}


MStateEn0Csr::MStateEn0Csr(RiscVState *state)
    : RiscVSimpleCsr<uint64_t>("mstateen0", 0x30C, 0ULL, -1ULL, -1ULL, state) {}

void MStateEn0Csr::Write(uint64_t value) {
  Set(value);
}

void MStateEn0Csr::Write(uint32_t value) {
  Write(static_cast<uint64_t>(value));
}

SStateEn0Csr::SStateEn0Csr(RiscVState *state)
    : RiscVSimpleCsr<uint64_t>("sstateen0", 0x10C, 0ULL, -1ULL, -1ULL, state) {}

void SStateEn0Csr::Write(uint64_t value) {
  Set(value);
}

void SStateEn0Csr::Write(uint32_t value) {
  Write(static_cast<uint64_t>(value));
}
uint32_t RiscVSimModeCsr::GetUint32() {
  return static_cast<uint32_t>(state_->privilege_mode());
}

uint64_t RiscVSimModeCsr::GetUint64() {
  return static_cast<uint64_t>(state_->privilege_mode());
}

void RiscVSimModeCsr::Set(uint32_t value) {
  state_->set_privilege_mode(static_cast<PrivilegeMode>(value));
}

void RiscVSimModeCsr::Set(uint64_t value) {
  state_->set_privilege_mode(static_cast<PrivilegeMode>(value));
}

}  // namespace riscv
}  // namespace sim
}  // namespace mpact
