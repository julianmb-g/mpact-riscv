#ifndef MPACT_RISCV_RISCV_RISCV_DTB_LOADER_H_
#define MPACT_RISCV_RISCV_RISCV_DTB_LOADER_H_

#include <string>
#include "absl/status/status.h"
#include "riscv/riscv_state.h"

namespace mpact {
namespace sim {
namespace riscv {

class RiscvDtbLoader {
 public:
  // Boot sequence protocol contract:
  // 1. Maps vmlinux payload to 0x20000000.
  // 2. Maps .dtb payload to 0x203F0000.
  // 3. Seeds M-Mode hart_id into a0.
  // 4. Seeds physical address of the device tree blob (.dtb) into a1.
  // Returns absl::NotFoundError if the pre-compiled payload is missing to prevent panic.
  static absl::Status LoadFirmwareAndSeedRegisters(
      RiscVState* state, 
      const std::string& vmlinux_payload_path, 
      const std::string& dtb_payload_path);
};

}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_RISCV_RISCV_DTB_LOADER_H_
