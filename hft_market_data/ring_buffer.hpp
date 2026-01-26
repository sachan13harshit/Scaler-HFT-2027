#pragma once
#include <atomic>
#include <cstddef>

constexpr size_t RING_SIZE = 1024;
constexpr size_t CACHE_LINE = 64;

/**
 * Lock-free Single Producer Single Consumer (SPSC) Ring Buffer
 *
 * Memory ordering:
 * - Producer: relaxed load on write_idx, acquire on read_idx, release store
 * - Consumer: relaxed load on read_idx, acquire on write_idx, release store
 *
 * Cache-line padding (alignas(64)) prevents false sharing between producer and
 * consumer
 */
template <typename T, size_t Size = RING_SIZE>
struct alignas(CACHE_LINE) SPSCRingBuffer {
  alignas(CACHE_LINE) std::atomic<size_t> write_idx{0};
  alignas(CACHE_LINE) std::atomic<size_t> read_idx{0};
  alignas(CACHE_LINE) T buffer[Size];

  bool push(const T &data) {
    size_t write = write_idx.load(std::memory_order_relaxed);
    size_t next = (write + 1) % Size;
    if (next == read_idx.load(std::memory_order_acquire)) {
      return false; // Buffer full
    }
    buffer[write] = data;
    write_idx.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T &out) {
    size_t read = read_idx.load(std::memory_order_relaxed);
    if (read == write_idx.load(std::memory_order_acquire)) {
      return false; // Buffer empty
    }
    out = buffer[read];
    read_idx.store((read + 1) % Size, std::memory_order_release);
    return true;
  }

  bool empty() const {
    return read_idx.load(std::memory_order_acquire) ==
           write_idx.load(std::memory_order_acquire);
  }

  size_t size() const {
    size_t w = write_idx.load(std::memory_order_acquire);
    size_t r = read_idx.load(std::memory_order_acquire);
    return (w + Size - r) % Size;
  }
};
