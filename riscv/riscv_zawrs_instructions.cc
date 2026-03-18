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

#include "riscv/riscv_zawrs_instructions.h"

#include <thread>

#include "mpact/sim/generic/instruction.h"

namespace mpact::sim::riscv {

using ::mpact::sim::generic::Instruction;

void RiscVWrsNto(const Instruction* inst) {
  // Implement Zawrs polling yield (Wait-on-Reservation-Set).
  // Without a multi-core reservation set tracked explicitly, this instruction 
  // falls back to yielding the host OS thread to prevent the simulator from 
  // busy-looping at 100% CPU.
  std::this_thread::yield();
}

void RiscVWrsSto(const Instruction* inst) {
  // WRS.STO acts the same as WRS.NTO in our single-threaded or relaxed
  // simulation environment, yielding the CPU to prevent starvation.
  std::this_thread::yield();
}

}  // namespace mpact::sim::riscv
