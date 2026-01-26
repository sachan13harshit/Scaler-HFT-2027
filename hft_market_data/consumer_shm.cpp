
#include "common.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <fcntl.h>
#include <fmt/format.h>
#include <limits>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

using Clock = std::chrono::high_resolution_clock;

static std::atomic<bool> g_running{true};
void signal_handler(int) { g_running = false; }

inline int64_t now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             Clock::now().time_since_epoch())
      .count();
}

inline std::string format_time(int64_t ns) {
  auto secs = ns / 1'000'000'000;
  auto nanos = ns % 1'000'000'000;
  std::time_t t = static_cast<std::time_t>(secs);
  std::tm *tm = std::localtime(&t);
  return fmt::format("[{:02d}:{:02d}:{:02d}.{:09d}]", tm->tm_hour, tm->tm_min,
                     tm->tm_sec, nanos);
}

RingBuffer *open_shm() {
  int fd = shm_open(SHM_NAME, O_RDWR, 0666);
  if (fd == -1)
    return nullptr;
  void *ptr = mmap(nullptr, sizeof(RingBuffer), PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd, 0);
  close(fd);
  return (ptr == MAP_FAILED) ? nullptr : reinterpret_cast<RingBuffer *>(ptr);
}

int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  fmt::print("{} === SHM Consumer Starting ===\n", format_time(now_ns()));

  // Open SHM
  RingBuffer *ring = nullptr;
  for (int i = 0; i < 10 && !ring && g_running; i++) {
    ring = open_shm();
    if (!ring) {
      fmt::print("{} Waiting for publisher... ({}/10)\n", format_time(now_ns()),
                 i + 1);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  if (!ring) {
    fmt::print(stderr, "Failed to connect to SHM\n");
    return 1;
  }
  fmt::print("{} Connected to SHM\n", format_time(now_ns()));

  // Latency tracking
  uint64_t count = 0;
  int64_t total_latency = 0;
  int64_t min_latency = std::numeric_limits<int64_t>::max();
  int64_t max_latency = 0;

  MarketData data;
  fmt::print("{} Consuming (Ctrl+C to stop)...\n", format_time(now_ns()));

  while (g_running) {
    if (ring->pop(data)) {
      int64_t recv_time = now_ns();
      int64_t latency = recv_time - data.timestamp_ns;

      // Update stats
      total_latency += latency;
      min_latency = std::min(min_latency, latency);
      max_latency = std::max(max_latency, latency);
      count++;

      // Log message
      fmt::print("{} {} BID={:.2f} ASK={:.2f}\n",
                 format_time(data.timestamp_ns), data.instrument, data.bid,
                 data.ask);
    } else {
      std::this_thread::yield();
    }
  }

  // Print latency stats
  fmt::print("\n{} === Performance Results ===\n", format_time(now_ns()));
  fmt::print("Messages: {}\n", count);
  if (count > 0) {
    double avg = static_cast<double>(total_latency) / count;
    fmt::print("Avg Latency: {:.2f} µs\n", avg / 1000.0);
    fmt::print("Min Latency: {} ns\n", min_latency);
    fmt::print("Max Latency: {} µs\n", max_latency / 1000);
  }

  munmap(ring, sizeof(RingBuffer));
  return 0;
}
