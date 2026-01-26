#pragma once
#include <cstddef>
#include <cstdint>

struct MarketData {
  char instrument[16];
  double bid;
  double ask;
  int64_t timestamp_ns;
};

constexpr const char *SHM_NAME = "/hft_mkt";
constexpr uint16_t TCP_PORT = 9000;

#include "ring_buffer.hpp"

using RingBuffer = SPSCRingBuffer<MarketData, RING_SIZE>;
