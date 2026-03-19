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

#ifndef MPACT_RISCV_RISCV_RISCV_XSTATUS_H_
#define MPACT_RISCV_RISCV_RISCV_XSTATUS_H_

#include <cstdint>

#include "mpact/sim/generic/arch_state.h"
#include "riscv/riscv_csr.h"
#include "riscv/riscv_misa.h"

// This file defines the classes that implement the hardware status CSR for
// machine mode (mstatus), supervisor mode (sstatus), and user mode (ustatus).
// The implementation is derived from RiscVCsr<uint64_t>

namespace mpact {
namespace sim {
namespace riscv {

using ::mpact::sim::generic::ArchState;

class RiscVMStatus : public RiscVSimpleCsr<uint64_t> {
 public:
  // Read/Write masks.
  static constexpr uint64_t kReadMask = 0x8000'000f'007f'f9bbULL;
  static constexpr uint64_t kWriteMask = 0x0000'0000'007f'f9bbULL;
  // Disable default constructor.
  RiscVMStatus() = delete;
  RiscVMStatus(uint32_t initial_value, ArchState* state, RiscVMIsa* misa);
  RiscVMStatus(uint64_t initial_value, ArchState* state, RiscVMIsa* misa);

  ~RiscVMStatus() override = default;

  // RiscVSimpleCsr<uint64_t> method overrides.
  uint32_t AsUint32() override;
  void Write(uint32_t value) override;
  void SetBits(uint32_t bits) override;
  void ClearBits(uint32_t bits) override;
  uint32_t GetUint32() override;
  void Set(uint32_t value) override;
  void Set(uint64_t value) override;

  // Getters for state info.
  // Read only dirty bit.
  bool sd() { return GetterHelper<31, 0b1>() || GetterHelper<63, 0b1>(); }
  // SXL - supervisor xlen.
  int sxl() { return GetterHelper<34, 0b11>(); }
  // UXL - user xlen.
  int uxl() { return GetterHelper<32, 0b11>(); }
  // Trap SRET.
  bool tsr() { return GetterHelper<22, 0b1>(); }
  // TW - timeout wait (WFI).
  bool tw() { return GetterHelper<21, 0b1>(); }
  // TVM - trap virtual memory.
  bool tvm() { return GetterHelper<20, 0b1>(); }
  // MXR - make executable readable.
  bool mxr() { return GetterHelper<19, 0b1>(); }
  // SUM - supervisor user memory access.
  bool sum() { return GetterHelper<18, 0b1>(); }
  // MPRV - modify privilege bit.
  bool mprv() { return GetterHelper<17, 0b1>(); }
  // XS - extension state dirty.
  int xs() { return GetterHelper<15, 0b11>(); }
  // FS - fp state dirty.
  int fs() { return GetterHelper<13, 0b11>(); }
  // MPP - machine previous privilege level.
  int mpp() { return GetterHelper<11, 0b11>(); }
  // SPP - supervisor previous privilege level.
  int spp() { return GetterHelper<8, 0b1>(); }
  // Previous interrupt enable for machine, supervisor, and user level.
  bool mpie() { return GetterHelper<7, 0b1>(); }
  bool spie() { return GetterHelper<5, 0b1>(); }
  bool upie() { return GetterHelper<4, 0b1>(); }
  // Interrupt enable for machine, supervisor, and user level.
  bool mie() { return GetterHelper<3, 0b1>(); }
  bool sie() { return GetterHelper<1, 0b1>(); }
  bool uie() { return GetterHelper<0, 0b1>(); }

  // Setters for state info. Using uint32_t since each field is only a bit or
  // two. These set a value in a buffer. The buffer has to be submitted after
  // the writes so that all the set_* get batched into one update.
  void set_tsr(uint32_t value) { SetterHelper<22, 0b1>(value); }
  void set_tw(uint32_t value) { SetterHelper<21, 0b1>(value); }
  void set_tvm(uint32_t value) { SetterHelper<20, 0b1>(value); }
  void set_mxr(uint32_t value) { SetterHelper<19, 0b1>(value); }
  void set_sum(uint32_t value) { SetterHelper<18, 0b1>(value); }
  void set_mprv(uint32_t value) { SetterHelper<17, 0b1>(value); }
  void set_xs(uint32_t value) { SetterHelper<15, 0b11>(value); }
  void set_fs(uint32_t value) { SetterHelper<13, 0b11>(value); }
  void set_mpp(uint32_t value);
  void set_spp(uint32_t value) { SetterHelper<8, 0b1>(value); }
  void set_mpie(uint32_t value) { SetterHelper<7, 0b1>(value); }
  void set_spie(uint32_t value) { SetterHelper<5, 0b1>(value); }
  void set_upie(uint32_t value) { SetterHelper<4, 0b1>(value); }
  void set_mie(uint32_t value) { SetterHelper<3, 0b1>(value); }
  void set_sie(uint32_t value) { SetterHelper<1, 0b1>(value); }
  void set_uie(uint32_t value) { SetterHelper<0, 0b1>(value); }
  void Submit() {
    uint64_t new_value = (GetUint64() & ~buffer_mask_) | buffer_;
    Write(new_value);
    buffer_ = 0;
    buffer_mask_ = 0;
  }

 private:
  // Private constructor.
  RiscVMStatus(uint64_t initial_value, ArchState* state, RiscVXlen xlen,
               RiscVMIsa* misa);
  // Template function to help implement the getters.
  template <int Shift, uint64_t BitMask>
  inline int GetterHelper() {
    return (GetUint64() >> Shift) & BitMask;
  }
  // Template function to help implement the setters.
  template <int Shift, uint64_t BitMask>
  inline void SetterHelper(uint32_t value) {
    buffer_ = (buffer_ & ~(BitMask << Shift)) | ((value & BitMask) << Shift);
    buffer_mask_ |= BitMask << Shift;
  }

  RiscVMIsa* misa_;
  uint64_t buffer_ = 0;
  uint64_t buffer_mask_ = 0;
  uint32_t read_mask_32_;
  uint32_t write_mask_32_;
  uint64_t set_mask_from_32_;
};

// The sstatus register is just a restricted subset of mstatus, so the real
// values are contained in mstatus. This provides really just an interface
// to access the restricted subset.
class RiscVSStatus : public RiscVSimpleCsr<uint64_t> {
 public:
  static constexpr uint64_t kReadMask = 0x8000'0003'000d'e133ULL;
  static constexpr uint64_t kWriteMask = 0x0000'0000'000d'e133ULL;

  RiscVSStatus() = delete;
  RiscVSStatus(RiscVMStatus* mstatus, RiscVState* state);

  // Overrides.
  uint64_t AsUint64() override;
  uint32_t AsUint32() override;
  void Write(uint32_t value) override;
  void SetBits(uint32_t bits) override;
  void ClearBits(uint32_t bits) override;
  void Set(uint32_t value) override;
  void Set(uint64_t value) override;
  uint32_t GetUint32() override;
  uint64_t GetUint64() override;

  // Getters for state info.
  // Read only dirty bit.
  bool sd() { return mstatus_->sd(); }
  // UXL - user xlen.
  int uxl() { return mstatus_->uxl(); }
  // MXR - make executable readable.
  bool mxr() { return mstatus_->mxr(); }
  // SUM - supervisor user memory access.
  bool sum() { return mstatus_->sum(); }
  // XS - extension state dirty.
  int xs() { return mstatus_->xs(); }
  // FS - fp state dirty.
  int fs() { return mstatus_->fs(); }
  // SPP - supervisor previous privilege level.
  int spp() { return mstatus_->spp(); }
  // Previous interrupt enable for machine, supervisor, and user level.
  bool spie() { return mstatus_->spie(); }
  bool upie() { return mstatus_->upie(); }
  // Interrupt enable for machine, supervisor, and user level.
  bool sie() { return mstatus_->sie(); }
  bool uie() { return mstatus_->uie(); }

  // Setters for state info. Using uint32_t since each field is only a bit or
  // two.
  void set_mxr(uint32_t value) { mstatus_->set_mxr(value); }
  void set_sum(uint32_t value) { mstatus_->set_sum(value); }
  void set_xs(uint32_t value) { mstatus_->set_xs(value); }
  void set_fs(uint32_t value) { mstatus_->set_fs(value); }
  void set_spp(uint32_t value) { mstatus_->set_spp(value); }
  void set_spie(uint32_t value) { mstatus_->set_spie(value); }
  void set_upie(uint32_t value) { mstatus_->set_upie(value); }
  void set_sie(uint32_t value) { mstatus_->set_sie(value); }
  void set_uie(uint32_t value) { mstatus_->set_uie(value); }
  void Submit() { mstatus_->Submit(); }

 private:
  uint32_t read_mask_32_;
  uint32_t write_mask_32_;
  uint64_t set_mask_from_32_;
  RiscVMStatus* mstatus_;
};

// The ustatus register is a further restricted view of sstatus.
class RiscVUStatus : public RiscVSimpleCsr<uint64_t> {
 public:
  static constexpr uint64_t kReadMask = 0x11ULL;
  static constexpr uint64_t kWriteMask = 0x11ULL;

  RiscVUStatus() = delete;
  RiscVUStatus(RiscVMStatus* mstatus, RiscVState* state);
  // Overrides.
  uint64_t AsUint64() override;
  uint32_t AsUint32() override;
  void Write(uint32_t value) override;
  void SetBits(uint32_t bits) override;
  void ClearBits(uint32_t bits) override;
  void Set(uint32_t value) override;
  void Set(uint64_t value) override;
  uint32_t GetUint32() override;
  uint64_t GetUint64() override;

  // Accessors.
  bool uie() { return mstatus_->uie(); }
  void set_uie(uint32_t value) { mstatus_->set_uie(value); }

 private:
  uint32_t read_mask_32_;
  uint32_t write_mask_32_;
  uint64_t set_mask_from_32_;
  RiscVMStatus* mstatus_;
};

// The satp register.
template <typename T>
class RiscVSAtp : public RiscVSimpleCsr<T> {
 public:
  explicit RiscVSAtp(ArchState* state)
      : RiscVSimpleCsr<T>("satp", RiscVCsrEnum::kSAtp, 0, state) {}

  void Write(uint32_t value) override {
    if constexpr (std::is_same_v<T, uint32_t>) {
      // For RV32, MODE is bit 31. Allowed modes: 0 (Bare), 1 (Sv32).
      // Since it's a 1-bit field, it can only be 0 or 1, so all writes are valid.
      RiscVSimpleCsr<T>::Write(value);
    } else {
      // Writing 32-bit value to a 64-bit register clears upper 32 bits.
      // Mode (bits 63..60) becomes 0, which is valid.
      RiscVSimpleCsr<T>::Write(value);
    }
  }

  void Write(uint64_t value) override {
    if constexpr (std::is_same_v<T, uint64_t>) {
      uint64_t mode = (value >> 60) & 0xF;
      // For RV64, 0 (Bare), 8 (Sv39), 9 (Sv48), and 10 (Sv57) are supported.
      if (mode != 0 && mode != 8 && mode != 9 && mode != 10) {
        return; // strictly ignored
      }
    }
    RiscVSimpleCsr<T>::Write(value);
  }
};

}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_RISCV_RISCV_XSTATUS_H_
