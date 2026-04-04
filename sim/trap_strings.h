#ifndef MPACT_RISCV_SIM_TRAP_STRINGS_H_
#define MPACT_RISCV_SIM_TRAP_STRINGS_H_

#include "absl/strings/string_view.h"

namespace mpact {
namespace sim {
namespace riscv {

constexpr absl::string_view kWatchdogTimeoutExceeded =
    "Watchdog timeout exceeded: Simulator failed to halt within the maximum allowed cycles.";

constexpr uint64_t TL_MAX_TIMEOUT_CYCLES = 120000000;

}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_SIM_TRAP_STRINGS_H_
