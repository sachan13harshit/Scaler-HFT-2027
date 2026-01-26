/**
 * Process A - Market Data Publisher
 * Publishes via TCP (127.0.0.1:9000) and Shared Memory ring buffer
 */
#include "common.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <fmt/format.h>
#include <list>
#include <memory>
#include <random>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

using boost::asio::ip::tcp;
using Clock = std::chrono::high_resolution_clock;

static std::atomic<bool> g_running{true};
void signal_handler(int) { g_running = false; }

// Get nanosecond timestamp
inline int64_t now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             Clock::now().time_since_epoch())
      .count();
}

// Format timestamp for logging
inline std::string format_time(int64_t ns) {
  auto secs = ns / 1'000'000'000;
  auto nanos = ns % 1'000'000'000;
  std::time_t t = static_cast<std::time_t>(secs);
  std::tm *tm = std::localtime(&t);
  return fmt::format("[{:02d}:{:02d}:{:02d}.{:09d}]", tm->tm_hour, tm->tm_min,
                     tm->tm_sec, nanos);
}

// TCP Session
class Session : public std::enable_shared_from_this<Session> {
public:
  explicit Session(tcp::socket socket) : socket_(std::move(socket)) {
    socket_.set_option(tcp::no_delay(true));
  }
  tcp::socket &socket() { return socket_; }

  void send(const std::string &msg) {
    auto self = shared_from_this();
    auto data = std::make_shared<std::string>(msg + "\n");
    boost::asio::async_write(
        socket_, boost::asio::buffer(*data),
        [self, data](boost::system::error_code, std::size_t) {});
  }
  bool is_open() const { return socket_.is_open(); }

private:
  tcp::socket socket_;
};

// TCP Server
class Server {
public:
  Server(boost::asio::io_context &io, uint16_t port)
      : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    accept();
    fmt::print("{} TCP Server on port {}\n", format_time(now_ns()), port);
  }

  void broadcast(const std::string &msg) {
    sessions_.remove_if([](auto &s) { return !s->is_open(); });
    for (auto &s : sessions_)
      s->send(msg);
  }
  size_t clients() const { return sessions_.size(); }

private:
  void accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket sock) {
          if (!ec) {
            sock.set_option(tcp::no_delay(true));
            sessions_.push_back(std::make_shared<Session>(std::move(sock)));
            fmt::print("{} Client connected ({})\n", format_time(now_ns()),
                       sessions_.size());
          }
          accept();
        });
  }
  tcp::acceptor acceptor_;
  std::list<std::shared_ptr<Session>> sessions_;
};

// Create shared memory ring buffer
RingBuffer *create_shm() {
  shm_unlink(SHM_NAME);
  int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    perror("shm_open");
    return nullptr;
  }
  ftruncate(fd, sizeof(RingBuffer));
  void *ptr = mmap(nullptr, sizeof(RingBuffer), PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd, 0);
  close(fd);
  if (ptr == MAP_FAILED) {
    perror("mmap");
    return nullptr;
  }
  return new (ptr) RingBuffer();
}

int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  fmt::print("{} === Publisher Starting ===\n", format_time(now_ns()));

  // Create SHM
  RingBuffer *ring = create_shm();
  if (!ring)
    return 1;
  fmt::print("{} SHM ring buffer created\n", format_time(now_ns()));

  // TCP Server
  boost::asio::io_context io;
  Server server(io, TCP_PORT);
  std::thread io_thread([&io]() { io.run(); });

  // Market data generator
  std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<double> dist(-2.0, 2.0);

  uint64_t count = 0;
  const double base_price = 2850.0;

  fmt::print("{} Publishing (Ctrl+C to stop)...\n", format_time(now_ns()));

  while (g_running) {
    MarketData data;
    std::strncpy(data.instrument, "RELIANCE", sizeof(data.instrument));
    double mid = base_price + dist(gen);
    data.bid = mid - 0.25;
    data.ask = mid + 0.25;
    data.timestamp_ns = now_ns();

    // Push to SHM
    ring->push(data);

    // Broadcast to TCP
    std::string json = fmt::format(
        R"({{"instrument":"{}","bid":{:.2f},"ask":{:.2f},"timestamp_ns":{}}})",
        data.instrument, data.bid, data.ask, data.timestamp_ns);
    boost::asio::post(io, [&server, json]() { server.broadcast(json); });

    count++;
    if (count % 10000 == 0) {
      fmt::print("{} Published {} msgs (clients: {}, shm: {})\n",
                 format_time(now_ns()), count, server.clients(), ring->size());
    }
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }

  fmt::print("{} Shutdown (total: {})\n", format_time(now_ns()), count);
  io.stop();
  io_thread.join();
  shm_unlink(SHM_NAME);
  return 0;
}
