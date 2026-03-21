#ifndef MPACT_RISCV_RISCV_TRACE_FORMATTER_H_
#define MPACT_RISCV_RISCV_TRACE_FORMATTER_H_

#include <string>
#include "mpact/sim/generic/data_buffer.h"

namespace mpact {
namespace sim {
namespace riscv {

class TraceFormatter {
 public:
  static void AppendDataBufferValue(std::string* trace_str, const std::string& name, mpact::sim::generic::DataBuffer* db);
};

}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_RISCV_TRACE_FORMATTER_H_
