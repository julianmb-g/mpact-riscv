#ifndef MPACT_RISCV_RISCV_RVVI_SIM_H_
#define MPACT_RISCV_RISCV_RVVI_SIM_H_

#include <cstdint>
#include <atomic>
#include <thread>
#include <stdexcept>

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
      std::this_thread::yield();
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

extern "C" {
  void ClearTransientInstructionBuffer(uint32_t hartId);
}

}  // namespace rvvi
}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_RISCV_RVVI_SIM_H_
