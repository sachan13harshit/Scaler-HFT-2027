# HFT Market Data Publishing System

Low-latency market data publisher in C++17 using TCP and shared memory.

## Files
```
hft_market_data/
├── CMakeLists.txt       # Build configuration
├── common.hpp           # MarketData struct, constants
├── ring_buffer.hpp      # Lock-free SPSC ring buffer
├── publisher.cpp        # Process A - TCP + SHM publisher
├── consumer_shm.cpp     # Process B - SHM consumer
└── consumer_tcp.cpp     # Process C - TCP consumer
```

## Build
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

## Run
```bash
# Terminal 1: Start publisher
./publisher

# Terminal 2: Start SHM consumer
./consumer_shm

# Terminal 3: Start TCP consumer
./consumer_tcp
```

Press `Ctrl+C` to stop and see performance stats.

---

## Features Implemented

### Core Requirements ✅
| Feature | Implementation |
|---------|---------------|
| C++17 | CMakeLists.txt |
| Process A - Publisher | `publisher.cpp` |
| Process B - SHM Consumer | `consumer_shm.cpp` |
| Process C - TCP Consumer | `consumer_tcp.cpp` |
| fmt logging (no std::cout) | All files use `fmt::print()` |
| Optimized clocks | `std::chrono::high_resolution_clock` |

### TCP Server (4.1) ✅
| Feature | Implementation |
|---------|---------------|
| Boost.Asio | `#include <boost/asio.hpp>` |
| Loopback (127.0.0.1:9000) | `TCP_PORT = 9000` |
| JSON messages | `{"instrument":"RELIANCE","bid":...}` |
| TCP_NODELAY (Nagle disabled) | `socket_.set_option(tcp::no_delay(true))` |
| Non-blocking sockets | Async I/O with Boost.Asio |

### Shared Memory Queue (4.2) ✅
| Feature | Implementation |
|---------|---------------|
| SPSC Ring Buffer | `ring_buffer.hpp` |
| `shm_open()` / `mmap()` | `publisher.cpp`, `consumer_shm.cpp` |
| `std::atomic` | `std::atomic<size_t> write_idx, read_idx` |
| Memory ordering | `acquire`, `release`, `relaxed` |
| No locks/mutexes | Pure lock-free |
| Cache-line padding | `alignas(64)` on indices |

### Bonus Points ✅
| Bonus | Implementation |
|-------|---------------|
| Nanosecond timestamps | `now_ns()` using high_resolution_clock |
| Memory orders usage | acquire/release/relaxed semantics |
| Latency measurement | Performance stats on consumer exit |

---

## Log Format

### SHM Consumer
```
[11:35:18.478545625] RELIANCE BID=2850.47 ASK=2850.97
```

### TCP Consumer
```
[11:35:12.481955500] RELIANCE BID=2848.23 ASK=2848.73
```

### Performance Stats (on Ctrl+C)
```
=== Performance Results ===
Messages: 46959
Avg Latency: 2.15 µs
Min Latency: 41 ns
Max Latency: 162 µs
```

---

## Performance Results

| Metric       | SHM         | TCP         |
|--------------|-------------|-------------|
| Avg Latency  | ~2 µs       | ~15 µs      |
| Min Latency  | 41 ns       | 7 µs        |
| Max Latency  | ~162 µs     | ~4.4 ms     |
| Throughput   | 10,000 msg/s| 10,000 msg/s|

---

## Market Data Format
```json
{"instrument":"RELIANCE","bid":2850.25,"ask":2850.75,"timestamp_ns":1234567890123}
```
