#include "absl/time/time.h"
#include "absl/time/clock.h"
#ifndef MPACT_RISCV_RISCV_RVVI_SIM_H_
#define MPACT_RISCV_RISCV_RVVI_SIM_H_

#include <cstdint>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <mutex>
#include "absl/synchronization/mutex.h"
#include <condition_variable>
#include <sched.h>
#include <chrono>
#include "mpact/sim/util/memory/memory_interface.h"
#include "mpact/sim/generic/data_buffer.h"
#include <vector>

namespace mpact {
namespace sim {
namespace riscv {
namespace rvvi {

struct TracePacket {
  uint64_t pc;
  uint32_t instruction;
  bool valid;
  uint8_t padding[3];
};
static_assert(sizeof(TracePacket) == 16, "ABI Violation: TracePacket must be exactly 16 bytes for SPSC Ring Buffer alignment");

template <typename T, size_t Capacity>
class SpscRingBuffer {
 public:
  SpscRingBuffer() : head_(0), tail_(0), abort_(false) {}

  void Push(const T& item) {
    size_t next_head = (head_.load(std::memory_order_relaxed) + 1) % Capacity;
    auto start_time = std::chrono::steady_clock::now();
    constexpr int kSpscYieldTimeoutMs = 5000;
    const auto timeout_duration = std::chrono::milliseconds(kSpscYieldTimeoutMs);
    // Explicit backpressure handling (blocking yield) with abort safety
    while (next_head == tail_.load(std::memory_order_acquire)) {
      if (abort_.load(std::memory_order_acquire)) {
        throw std::runtime_error("SPSC Ring Buffer aborted due to consumer failure");
      }
      if (std::chrono::steady_clock::now() - start_time > timeout_duration) {
        throw std::runtime_error("SPSC Formatting Daemon Deadlock");
      }
      absl::SleepFor(absl::Milliseconds(1));
    }
    buffer_[head_.load(std::memory_order_relaxed)] = item;
    head_.store(next_head, std::memory_order_release);
  }

  bool Pop(T& item) {
    size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false; // Empty
    }
    item = buffer_[tail];
    tail_.store((tail + 1) % Capacity, std::memory_order_release);
    return true;
  }

  bool Empty() const {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
  }

  // Called only by the producer (simulator thread)
  void Clear() {
    // SPSC safe clear: Producer abandons all pushed items by rolling head back to tail
    head_.store(tail_.load(std::memory_order_acquire), std::memory_order_release);
  }

  void Abort() {
    abort_.store(true, std::memory_order_release);
  }

 private:
  T buffer_[Capacity];
  std::atomic<size_t> head_;
  std::atomic<size_t> tail_;
  std::atomic<bool> abort_;
};

class RvviMemoryMapper : public util::MemoryInterface {

 public:

  struct AddressRange {

    uint64_t start;

    uint64_t end;

  };



  explicit RvviMemoryMapper(util::MemoryInterface* memory);

  ~RvviMemoryMapper() override;



  void AddMmioRange(uint64_t start, uint64_t end);
  void set_context(generic::Instruction* inst, generic::ReferenceCount* context) {
    current_inst_ = inst;
    current_context_ = context;
  }



  void Load(uint64_t address, generic::DataBuffer* db,

            generic::Instruction* inst, generic::ReferenceCount* context) override;

  void Load(generic::DataBuffer* address_db, generic::DataBuffer* mask_db,

            int el_size, generic::DataBuffer* db, generic::Instruction* inst,

            generic::ReferenceCount* context) override;

  void Store(uint64_t address, generic::DataBuffer* db) override;

  void Store(generic::DataBuffer* address_db, generic::DataBuffer* mask_db,

             int el_size, generic::DataBuffer* db) override;



 private:

  bool IsMmio(uint64_t address) const;

  void DoRmwStore(uint64_t address, generic::DataBuffer* db);

  absl::Mutex rmw_mutex_;

  util::MemoryInterface* memory_;

  std::vector<AddressRange> mmio_ranges_;

  generic::DataBufferFactory db_factory_;
  generic::Instruction* current_inst_ = nullptr;
  generic::ReferenceCount* current_context_ = nullptr;

};


extern SpscRingBuffer<TracePacket, 1024> g_trace_buffer;

class AsyncFormattingDaemon {
 public:
  explicit AsyncFormattingDaemon(int timeout_seconds)
      : timeout_seconds_(timeout_seconds), running_(false) {}

  ~AsyncFormattingDaemon() {
    Stop();
  }

  void Start();
  void Stop();

 private:
  void DaemonLoop();

  int timeout_seconds_;
  std::atomic<bool> running_;
  std::thread worker_thread_;
  std::mutex cv_m_;
  std::condition_variable cv_;
};

extern "C" {
  void ClearTransientInstructionBuffer(uint32_t hartId);
}

}  // namespace rvvi
}  // namespace riscv
}  // namespace sim
}  // namespace mpact

// MPACT_RISCV_RISCV_RVVI_SIM_H_
extern "C" void PushTracePacket(uint64_t pc, uint32_t inst, bool valid);

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RVVI_TRACE_EVENT_T_DEFINED
#define RVVI_TRACE_EVENT_T_DEFINED

typedef struct __attribute__((aligned(64))) {
    uint64_t timestamp;      // [0-7]
    uint64_t pc;             // [8-15]
    uint32_t hartId;         // [16-19]
    uint32_t order;          // [20-23]
    uint32_t insn;           // [24-27]
    uint32_t trap_cause;     // [28-31]
    uint64_t trap_tval;      // [32-39]
    uint64_t gpr_wdata;      // [40-47]
    uint64_t fpr_wdata;      // [48-55]
    uint64_t mem_paddr;      // [56-63]
    uint64_t mem_wdata;      // [64-71]
    uint64_t mem_rdata;      // [72-79]
    uint8_t  gpr_addr;       // [80]
    uint8_t  gpr_we;         // [81]
    uint8_t  fpr_addr;       // [82]
    uint8_t  fpr_we;         // [83]
    uint8_t  mem_wmask;      // [84]
    uint8_t  mem_rmask;      // [85]
    uint8_t  mem_we;         // [86]
    uint8_t  mem_re;         // [87]
    uint8_t  vr_addr;        // [88]
    uint8_t  vr_we;          // [89]
    uint16_t vstart;         // [90-91]
    uint32_t vl;             // [92-95]
    uint64_t vr_wdata_0;     // [96-103]
    uint64_t vr_wdata_1;     // [104-111]
    uint64_t vr_wdata_2;     // [112-119]
    uint64_t vr_wdata_3;     // [120-127]
} rvvi_trace_event_t;

#endif // RVVI_TRACE_EVENT_T_DEFINED

#ifdef __cplusplus
}
#endif

#endif  // MPACT_RISCV_RISCV_RVVI_SIM_H_

