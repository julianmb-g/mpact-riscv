#ifndef MPACT_RISCV_RISCV_RISCV_SVINVAL_INSTRUCTIONS_H_
#define MPACT_RISCV_RISCV_RISCV_SVINVAL_INSTRUCTIONS_H_

#include "mpact/sim/generic/instruction.h"

namespace mpact {
namespace sim {
namespace riscv {

void RiscVSinvalVma(const generic::Instruction* inst);
void RiscVSfenceWInval(const generic::Instruction* inst);
void RiscVSfenceInvalIr(const generic::Instruction* inst);

}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_RISCV_RISCV_SVINVAL_INSTRUCTIONS_H_
