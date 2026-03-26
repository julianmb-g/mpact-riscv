#include <cstring>
#include <algorithm>
#include "riscv/rvvi_sim.h"

namespace mpact {
namespace sim {
namespace riscv {
namespace rvvi {

using ::mpact::sim::generic::DataBuffer;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::ReferenceCount;

SpscRingBuffer<TracePacket, 1024> g_trace_buffer;

extern "C" {
  void ClearTransientInstructionBuffer(uint32_t hartId) {
    (void)hartId;
    g_trace_buffer.Clear();
  }
}

void AsyncFormattingDaemon::Start() {
  if (running_.exchange(true)) {
    return;
  }
  worker_thread_ = std::thread(&AsyncFormattingDaemon::DaemonLoop, this);
}

void AsyncFormattingDaemon::Stop() {
  {
    std::lock_guard<std::mutex> lock(cv_m_);
    if (!running_.exchange(false)) {
      return;
    }
    cv_.notify_all();       // Instantly wake up the daemon if it's waiting
  }
  g_trace_buffer.Abort(); // Unblock ring buffer
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}

void AsyncFormattingDaemon::DaemonLoop() {
  (void)timeout_seconds_;
  
  while (running_.load()) {
    TracePacket packet;
    if (g_trace_buffer.Pop(packet)) {
      // Process packet here. (Dummy processing for scaffolding)
      (void)packet;
    } else {
      std::unique_lock<std::mutex> lock(cv_m_);
      cv_.wait_for(lock, std::chrono::milliseconds(1), [this] { return !running_.load(); });
    }
  }
}

RvviMemoryMapper::RvviMemoryMapper(util::MemoryInterface* memory)
    : memory_(memory), db_factory_() {}

RvviMemoryMapper::~RvviMemoryMapper() {
  db_factory_.Clear();
}

void RvviMemoryMapper::AddMmioRange(uint64_t start, uint64_t end) {
  mmio_ranges_.push_back({start, end});
}

bool RvviMemoryMapper::IsMmio(uint64_t address) const {
  for (const auto& range : mmio_ranges_) {
    if (address >= range.start && address < range.end) {
      return true;
    }
  }
  return false;
}

void RvviMemoryMapper::Load(uint64_t address, DataBuffer* db, Instruction* inst,
                            ReferenceCount* context) {
  memory_->Load(address, db, inst, context);
}

void RvviMemoryMapper::Load(DataBuffer* address_db, DataBuffer* mask_db,
                            int el_size, DataBuffer* db, Instruction* inst,
                            ReferenceCount* context) {
  memory_->Load(address_db, mask_db, el_size, db, inst, context);
}

void RvviMemoryMapper::Store(uint64_t address, DataBuffer* db) {
  if (IsMmio(address)) {
    memory_->Store(address, db);
    return;
  }
  
  size_t size = db->size<uint8_t>();
  uint64_t start_aligned = address & ~0x7ull;
  uint64_t end_aligned = (address + size - 1) & ~0x7ull;
  
  if (start_aligned == end_aligned) {
    // Fits perfectly within an 8-byte word; no RMW needed across 64-bit boundaries
    memory_->Store(address, db);
    return;
  }
  
  DoRmwStore(address, db);
}

void RvviMemoryMapper::Store(DataBuffer* address_db, DataBuffer* mask_db,
                             int el_size, DataBuffer* db) {
  auto mask_span = mask_db->Get<bool>();
  auto address_span = address_db->Get<uint64_t>();
  bool gather = address_span.size() > 1;
  
  size_t num_elements = mask_span.size();
  size_t db_size = db->size<uint8_t>();
  
  if (gather && address_span.size() < num_elements) num_elements = address_span.size();
  if (db_size < num_elements * el_size) num_elements = db_size / el_size;
  
  for (size_t i = 0; i < num_elements; i++) {
    if (!mask_span[i]) continue;
    uint64_t address = gather ? address_span[i] : address_span[0] + i * el_size;
    
    DataBuffer* elem_db = db_factory_.Allocate<uint8_t>(el_size);
    std::memcpy(elem_db->raw_ptr(), 
                static_cast<uint8_t*>(db->raw_ptr()) + i * el_size, el_size);
                
    Store(address, elem_db);
    elem_db->DecRef();
  }
}

void RvviMemoryMapper::DoRmwStore(uint64_t address, DataBuffer* db) {
  size_t size = db->size<uint8_t>();
  uint8_t* write_data = static_cast<uint8_t*>(db->raw_ptr());
  
  // Evaluate the entire range to prevent partial memory corruption
  uint64_t check_addr = address;
  size_t check_rem = size;
  while (check_rem > 0) {
    uint64_t aligned_addr = check_addr & ~0x7ull;
    size_t in_block_offset = check_addr & 0x7ull;
    size_t to_write = std::min(check_rem, (size_t)(8 - in_block_offset));
    if (IsMmio(aligned_addr)) {
      throw std::runtime_error("Read-Modify-Write cycle on MMIO base address");
    }
    check_addr += to_write;
    check_rem -= to_write;
  }

  uint64_t current_addr = address;
  size_t remaining = size;
  size_t offset = 0;
  
  absl::MutexLock lock(&rmw_mutex_);

  while (remaining > 0) {
    uint64_t aligned_addr = current_addr & ~0x7ull;
    size_t in_block_offset = current_addr & 0x7ull;
    size_t to_write = std::min(remaining, (size_t)(8 - in_block_offset));
    
    DataBuffer* block_db = db_factory_.Allocate<uint64_t>(1);
    
    // Read 64-bit block
    memory_->Load(aligned_addr, block_db, current_inst_, current_context_);
    
    // Modify bytes
    uint8_t* block_data = static_cast<uint8_t*>(block_db->raw_ptr());
    std::memcpy(block_data + in_block_offset, write_data + offset, to_write);
    
    // Write 64-bit block
    memory_->Store(aligned_addr, block_db);
    
    block_db->DecRef();
    
    current_addr += to_write;
    offset += to_write;
    remaining -= to_write;
  }
}

}  // namespace rvvi
}  // namespace riscv
}  // namespace sim
}  // namespace mpact

extern "C" void PushTracePacket(uint64_t pc, uint32_t inst, bool valid) {
  mpact::sim::riscv::rvvi::TracePacket packet;
  packet.pc = pc;
  packet.instruction = inst;
  packet.valid = valid;
  mpact::sim::riscv::rvvi::g_trace_buffer.Push(packet);
}
