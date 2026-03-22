#ifndef MPACT_RISCV_RISCV_RVVI_SIM_H_
#define MPACT_RISCV_RISCV_RVVI_SIM_H_

#include <cstdint>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include <sched.h>
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
};

template <typename T, size_t Capacity>
class SpscRingBuffer {
 public:
  SpscRingBuffer() : head_(0), tail_(0), abort_(false) {}

  void Push(const T& item) {
    size_t next_head = (head_.load(std::memory_order_relaxed) + 1) % Capacity;
    // Explicit backpressure handling (blocking yield) with abort safety
    while (next_head == tail_.load(std::memory_order_acquire)) {
      if (abort_.load(std::memory_order_acquire)) {
        throw std::runtime_error("SPSC Ring Buffer aborted due to consumer failure");
      }
      sched_yield();
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



  util::MemoryInterface* memory_;

  std::vector<AddressRange> mmio_ranges_;

  generic::DataBufferFactory db_factory_;
  generic::Instruction* current_inst_ = nullptr;
  generic::ReferenceCount* current_context_ = nullptr;

};


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

#endif  // MPACT_RISCV_RISCV_RVVI_SIM_H_
extern "C" void PushTracePacket(uint64_t pc, uint32_t inst, bool valid);
