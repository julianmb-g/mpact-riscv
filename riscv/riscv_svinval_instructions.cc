#include "riscv/riscv_svinval_instructions.h"
#include "riscv/riscv_state.h"
#include "riscv/riscv_csr.h"

namespace mpact {
namespace sim {
namespace riscv {

using ::mpact::sim::generic::Instruction;

// sinval.vma is equivalent to sfence.vma with regard to privilege
static void SvinvalCheckAndTrap(const Instruction* inst) {
  auto* state = static_cast<RiscVState*>(inst->state());
  PrivilegeMode mode = state->privilege_mode();
  auto* mstatus = state->mstatus();
  if ((mode == PrivilegeMode::kUser) ||
      ((mode == PrivilegeMode::kSupervisor) && mstatus->tvm())) {
    state->Trap(/*is_interrupt=*/false, /*trap_value=*/0,
                (uint32_t)ExceptionCode::kIllegalInstruction, inst->address(), inst);
    return;
  }
}

void RiscVSinvalVma(const Instruction* inst) {
  SvinvalCheckAndTrap(inst);
  // TODO: Emulate precise TLB flush behavior.
}

void RiscVSfenceWInval(const Instruction* inst) {
  SvinvalCheckAndTrap(inst);
  // TODO: Fill in semantics.
}

void RiscVSfenceInvalIr(const Instruction* inst) {
  SvinvalCheckAndTrap(inst);
  // TODO: Fill in semantics.
}

}  // namespace riscv
}  // namespace sim
}  // namespace mpact
