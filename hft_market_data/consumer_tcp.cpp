/**
 * Process C - TCP Consumer with Latency Measurement
 * Connects to TCP server and logs with latency stats
 */
#include "common.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <csignal>
#include <fmt/format.h>
#include <limits>
#include <nlohmann/json.hpp>
#include <thread>

using boost::asio::ip::tcp;
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

class TcpConsumer {
public:
  TcpConsumer(boost::asio::io_context &io) : socket_(io), resolver_(io) {}

  bool connect() {
    try {
      auto eps = resolver_.resolve("127.0.0.1", std::to_string(TCP_PORT));
      boost::asio::connect(socket_, eps);
      socket_.set_option(tcp::no_delay(true));
      fmt::print("{} Connected to 127.0.0.1:{}\n", format_time(now_ns()),
                 TCP_PORT);
      return true;
    } catch (...) {
      return false;
    }
  }

  void start() { read_line(); }

  uint64_t count() const { return count_; }
  int64_t total_latency() const { return total_latency_; }
  int64_t min_latency() const { return min_latency_; }
  int64_t max_latency() const { return max_latency_; }

private:
  void read_line() {
    boost::asio::async_read_until(
        socket_, buf_, '\n', [this](boost::system::error_code ec, std::size_t) {
          if (!ec) {
            std::istream is(&buf_);
            std::string line;
            std::getline(is, line);
            process(line);
            read_line();
          }
        });
  }

  void process(const std::string &json_str) {
    try {
      int64_t recv_time = now_ns();
      auto j = nlohmann::json::parse(json_str);
      int64_t msg_time = j["timestamp_ns"].get<int64_t>();
      int64_t latency = recv_time - msg_time;

      total_latency_ += latency;
      min_latency_ = std::min(min_latency_, latency);
      max_latency_ = std::max(max_latency_, latency);
      count_++;

      fmt::print("{} {} BID={:.2f} ASK={:.2f}\n", format_time(msg_time),
                 j["instrument"].get<std::string>(), j["bid"].get<double>(),
                 j["ask"].get<double>());
    } catch (...) {
    }
  }

  tcp::socket socket_;
  tcp::resolver resolver_;
  boost::asio::streambuf buf_;
  uint64_t count_ = 0;
  int64_t total_latency_ = 0;
  int64_t min_latency_ = std::numeric_limits<int64_t>::max();
  int64_t max_latency_ = 0;
};

int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  fmt::print("{} === TCP Consumer Starting ===\n", format_time(now_ns()));

  boost::asio::io_context io;
  TcpConsumer consumer(io);

  // Connect with retry
  for (int i = 0; i < 10 && !consumer.connect() && g_running; i++) {
    fmt::print("{} Retrying... ({}/10)\n", format_time(now_ns()), i + 1);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  consumer.start();
  std::thread io_thread([&io]() { io.run(); });

  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  io.stop();
  io_thread.join();

  // Print latency stats
  fmt::print("\n{} === Performance Results ===\n", format_time(now_ns()));
  fmt::print("Messages: {}\n", consumer.count());
  if (consumer.count() > 0) {
    double avg =
        static_cast<double>(consumer.total_latency()) / consumer.count();
    fmt::print("Avg Latency: {:.2f} µs\n", avg / 1000.0);
    fmt::print("Min Latency: {} µs\n", consumer.min_latency() / 1000);
    fmt::print("Max Latency: {} µs\n", consumer.max_latency() / 1000);
  }

  return 0;
}
