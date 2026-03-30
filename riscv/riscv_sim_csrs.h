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

#ifndef MPACT_RISCV_RISCV_RISCV_SIM_CSRS_H_
#define MPACT_RISCV_RISCV_RISCV_SIM_CSRS_H_

#include <cstdint>
#include <string>
#include <functional>

#include "riscv/riscv_csr.h"
#include "riscv/riscv_state.h"

namespace mpact {
namespace sim {
namespace riscv {

// Mseccfg CSR implements the Rule Lock Bypass (RLB) and Machine Mode Lock (MML) policies.
class MseccfgCsr : public RiscVSimpleCsr<uint64_t> {
 public:
  MseccfgCsr(RiscVState *state);
  void Write(uint64_t value) override;
  void Write(uint32_t value) override;
};

// Sstc CSR implements the stimecmp register for supervisor-mode timer interrupts.
// This serves as a decoupled stub hardware timer callback.
class STimeCmpCsr : public RiscVSimpleCsr<uint64_t> {
 public:
  STimeCmpCsr(RiscVState *state, std::function<void(uint64_t)> timer_cb = nullptr);
  void Write(uint64_t value) override;
  void Write(uint32_t value) override;
  void set_timer_cb(std::function<void(uint64_t)> timer_cb) { timer_cb_ = std::move(timer_cb); }
 private:
  std::function<void(uint64_t)> timer_cb_;
};

// Smstateen CSRs implement the State Enable Extension.
class MStateEn0Csr : public RiscVSimpleCsr<uint64_t> {
 public:
  MStateEn0Csr(RiscVState *state);
  void Write(uint64_t value) override;
  void Write(uint32_t value) override;
};

class SStateEn0Csr : public RiscVSimpleCsr<uint64_t> {
 public:
  SStateEn0Csr(RiscVState *state);
  void Write(uint64_t value) override;
  void Write(uint32_t value) override;
};

// Smcntrpmf extension CSRs
class MCycleCfgCsr : public RiscVSimpleCsr<uint64_t> {
 public:
  MCycleCfgCsr(RiscVState *state);
  void Write(uint64_t value) override;
  void Write(uint32_t value) override;
};

class MInstRetCfgCsr : public RiscVSimpleCsr<uint64_t> {
 public:
  MInstRetCfgCsr(RiscVState *state);
  void Write(uint64_t value) override;
  void Write(uint32_t value) override;
};

class RiscVSimModeCsr : public RiscVSimpleCsr<uint32_t> {
 public:
  RiscVSimModeCsr(std::string name, RiscVCsrEnum index, RiscVState* state)
      : RiscVSimpleCsr<uint32_t>(name, index, 0x0, 0x3, 0x3, state),
        state_(state) {}

  uint32_t GetUint32() override;
  uint64_t GetUint64() override;
  void Set(uint32_t value) override;
  void Set(uint64_t value) override;

 private:
  RiscVState* state_;
};

}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_RISCV_RISCV_SIM_CSRS_H_
