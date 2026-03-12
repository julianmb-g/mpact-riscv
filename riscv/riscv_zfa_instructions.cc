// Copyright 2024 Google LLC
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

#include "riscv/riscv_zfa_instructions.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/register.h"
#include "riscv/riscv_fp_host.h"
#include "riscv/riscv_fp_info.h"
#include "riscv/riscv_instruction_helpers.h"
#include "riscv/riscv_i_instructions.h"
#include "riscv/riscv_register.h"
#include "riscv/riscv_state.h"

namespace mpact {
namespace sim {
namespace riscv {

using FPRegister = RVFpRegister;

namespace {

// FLI Constants Table
template <typename T>
T GetFliConstant(uint32_t rs1) {
  switch (rs1) {
    case 0: return -1.0;
    case 1: return std::numeric_limits<T>::min(); // Minimum positive normal
    case 2: return std::ldexp(static_cast<T>(1.0), -16);
    case 3: return std::ldexp(static_cast<T>(1.0), -15);
    case 4: return std::ldexp(static_cast<T>(1.0), -8);
    case 5: return std::ldexp(static_cast<T>(1.0), -7);
    case 6: return 0.0625;
    case 7: return 0.125;
    case 8: return 0.25;
    case 9: return 0.3125;
    case 10: return 0.375;
    case 11: return 0.4375;
    case 12: return 0.5;
    case 13: return 0.625;
    case 14: return 0.75;
    case 15: return 0.875;
    case 16: return 1.0;
    case 17: return 1.25;
    case 18: return 1.5;
    case 19: return 1.75;
    case 20: return 2.0;
    case 21: return 2.5;
    case 22: return 3.0;
    case 23: return 4.0;
    case 24: return 8.0;
    case 25: return 16.0;
    case 26: return 128.0;
    case 27: return 256.0;
    case 28: return std::ldexp(static_cast<T>(1.0), 15);
    case 29: {
        // For half precision, 2^16 overflows to +inf
        T val = std::ldexp(static_cast<T>(1.0), 16);
        return val;
    }
    case 30: return std::numeric_limits<T>::infinity();
    default: return *reinterpret_cast<const T*>(&FPTypeInfo<T>::kCanonicalNaN);
  }
}

template <typename T>
void RVFli(const Instruction* instruction) {
  uint32_t rs1 = generic::GetInstructionSource<uint32_t>(instruction, 0);
  T val = GetFliConstant<T>(rs1);
  auto* dest = instruction->Destination(0);
  auto* db = dest->AllocateDataBuffer();
  // We need to NaN box the value for the FP register.
  FPRegister::ValueType reg_val = std::numeric_limits<FPRegister::ValueType>::max();
  if constexpr (sizeof(T) < sizeof(FPRegister::ValueType)) {
      reg_val <<= sizeof(T) * 8;
  } else {
      reg_val = 0;
  }
  reg_val |= *reinterpret_cast<typename FPTypeInfo<T>::UIntType*>(&val);
  db->Set<FPRegister::ValueType>(0, reg_val);
  db->Submit();
}

template <typename T>
void RVFMinm(const Instruction* instruction) {
  RiscVBinaryNaNBoxOp<FPRegister::ValueType, T, T>(
      instruction, [instruction](T a, T b) -> T {
        if (FPTypeInfo<T>::IsNaN(a) || FPTypeInfo<T>::IsNaN(b)) {
          if (FPTypeInfo<T>::IsSNaN(a) || FPTypeInfo<T>::IsSNaN(b)) {
            auto* db = instruction->Destination(1)->AllocateDataBuffer();
            db->Set<uint32_t>(0, *FPExceptions::kInvalidOp);
            db->Submit();
          }
          return *reinterpret_cast<const T*>(&FPTypeInfo<T>::kCanonicalNaN);
        }
        T abs_a = std::abs(a);
        T abs_b = std::abs(b);
        if (abs_a < abs_b) return a;
        if (abs_b < abs_a) return b;
        return (a < b) ? a : b;
      });
}

template <typename T>
void RVFMaxm(const Instruction* instruction) {
  RiscVBinaryNaNBoxOp<FPRegister::ValueType, T, T>(
      instruction, [instruction](T a, T b) -> T {
        if (FPTypeInfo<T>::IsNaN(a) || FPTypeInfo<T>::IsNaN(b)) {
          if (FPTypeInfo<T>::IsSNaN(a) || FPTypeInfo<T>::IsSNaN(b)) {
            auto* db = instruction->Destination(1)->AllocateDataBuffer();
            db->Set<uint32_t>(0, *FPExceptions::kInvalidOp);
            db->Submit();
          }
          return *reinterpret_cast<const T*>(&FPTypeInfo<T>::kCanonicalNaN);
        }
        T abs_a = std::abs(a);
        T abs_b = std::abs(b);
        if (abs_a > abs_b) return a;
        if (abs_b > abs_a) return b;
        return (a > b) ? a : b;
      });
}

template <typename XRegister, typename T>
static inline void RVFleq(const Instruction* instruction) {
  RiscVBinaryNaNBoxOp<typename XRegister::ValueType,
                      typename XRegister::ValueType, T>(
      instruction,
      [instruction](T a, T b) -> typename XRegister::ValueType {
        if (FPTypeInfo<T>::IsNaN(a) || FPTypeInfo<T>::IsNaN(b)) {
          if (FPTypeInfo<T>::IsSNaN(a) || FPTypeInfo<T>::IsSNaN(b)) {
            auto* db = instruction->Destination(1)->AllocateDataBuffer();
            db->Set<uint32_t>(0, *FPExceptions::kInvalidOp);
            db->Submit();
          }
          return 0;
        }
        return a <= b ? 1 : 0;
      });
}

template <typename XRegister, typename T>
static inline void RVFltq(const Instruction* instruction) {
  RiscVBinaryNaNBoxOp<typename XRegister::ValueType,
                      typename XRegister::ValueType, T>(
      instruction,
      [instruction](T a, T b) -> typename XRegister::ValueType {
        if (FPTypeInfo<T>::IsNaN(a) || FPTypeInfo<T>::IsNaN(b)) {
          if (FPTypeInfo<T>::IsSNaN(a) || FPTypeInfo<T>::IsSNaN(b)) {
            auto* db = instruction->Destination(1)->AllocateDataBuffer();
            db->Set<uint32_t>(0, *FPExceptions::kInvalidOp);
            db->Submit();
          }
          return 0;
        }
        return a < b ? 1 : 0;
      });
}

template <typename T>
void RVFRound(const Instruction* instruction, bool set_inexact) {
  // Explicitly trap fround until rigorous architectural rounding is implemented.
  RiscVIllegalInstruction(instruction);
}

} // namespace

// Single precision implementations
void RiscVFliS(const Instruction* instruction) { RVFli<float>(instruction); }
void RiscVFMinmS(const Instruction* instruction) { RVFMinm<float>(instruction); }
void RiscVFMaxmS(const Instruction* instruction) { RVFMaxm<float>(instruction); }
void RiscVFRoundS(const Instruction* instruction) { RVFRound<float>(instruction, true); }
void RiscVFRoundnxS(const Instruction* instruction) { RVFRound<float>(instruction, false); }
void RiscVFleqS(const Instruction* instruction) { RVFleq<RV64Register, float>(instruction); }
void RiscVFltqS(const Instruction* instruction) { RVFltq<RV64Register, float>(instruction); }

// Double precision implementations
void RiscVFliD(const Instruction* instruction) { RVFli<double>(instruction); }
void RiscVFMinmD(const Instruction* instruction) { RVFMinm<double>(instruction); }
void RiscVFMaxmD(const Instruction* instruction) { RVFMaxm<double>(instruction); }
void RiscVFRoundD(const Instruction* instruction) { RVFRound<double>(instruction, true); }
void RiscVFRoundnxD(const Instruction* instruction) { RVFRound<double>(instruction, false); }
void RiscVFleqD(const Instruction* instruction) { RVFleq<RV64Register, double>(instruction); }
void RiscVFltqD(const Instruction* instruction) { RVFltq<RV64Register, double>(instruction); }

void RiscVFCvtmodWD(const Instruction* instruction) {
    RiscVIllegalInstruction(instruction);
}

// Half precision implementations
// Explicitly trapped until rigorous f16 emulation is mapped.
void RiscVFliH(const Instruction* instruction) { RiscVIllegalInstruction(instruction); }
void RiscVFMinmH(const Instruction* instruction) { RiscVIllegalInstruction(instruction); }
void RiscVFMaxmH(const Instruction* instruction) { RiscVIllegalInstruction(instruction); }
void RiscVFRoundH(const Instruction* instruction) { RiscVIllegalInstruction(instruction); }
void RiscVFRoundnxH(const Instruction* instruction) { RiscVIllegalInstruction(instruction); }
void RiscVFleqH(const Instruction* instruction) { RiscVIllegalInstruction(instruction); }
void RiscVFltqH(const Instruction* instruction) { RiscVIllegalInstruction(instruction); }

// RV32-specific Zfa instructions.
void RiscVFmvhXD(const Instruction* instruction) { RiscVIllegalInstruction(instruction); }
void RiscVFmvpDX(const Instruction* instruction) { RiscVIllegalInstruction(instruction); }

}  // namespace riscv
}  // namespace sim
}  // namespace mpact
