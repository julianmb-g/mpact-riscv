#include "riscv/trace_formatter.h"

#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/str_cat.h"
#include "mpact/sim/generic/data_buffer.h"

namespace mpact {
namespace sim {
namespace riscv {

void TraceFormatter::AppendDataBufferValue(std::string* trace_str, const std::string& name, mpact::sim::generic::DataBuffer* db) {
  if (db == nullptr) return;
  if (db->size<uint8_t>() == sizeof(uint64_t)) {
    absl::StrAppend(trace_str, " ", absl::StrFormat("%-3s", name), " 0x",
      absl::Hex(db->Get<uint64_t>(0), absl::PadSpec::kZeroPad16));
  } else if (db->size<uint8_t>() == sizeof(uint32_t)) {
    absl::StrAppend(trace_str, " ", absl::StrFormat("%-3s", name), " 0x",
      absl::Hex(db->Get<uint32_t>(0), absl::PadSpec::kZeroPad8));
  }
}

}  // namespace riscv
}  // namespace sim
}  // namespace mpact
