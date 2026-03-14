#include "riscv/riscv_zhintpause_instructions.h"

#include <thread>

#include "mpact/sim/generic/instruction.h"

namespace mpact::sim::riscv {

using ::mpact::sim::generic::Instruction;

void RiscVPause(const Instruction* inst) {
  // Implement Zawrs polling yield (mpause).
  // This yields the host OS thread to prevent the simulator from busy-looping
  // at 100% CPU when simulating spin-waits (e.g. WRS.NTO).
  std::this_thread::yield();
}

}  // namespace mpact::sim::riscv
