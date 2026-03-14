#include "riscv/rvvi_sim.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace rvvi {

SpscRingBuffer<TracePacket, 1024> g_trace_buffer;

extern "C" {
  void ClearTransientInstructionBuffer(uint32_t hartId) {
    (void)hartId;
    g_trace_buffer.Clear();
  }
}

}  // namespace rvvi
}  // namespace riscv
}  // namespace sim
}  // namespace mpact
