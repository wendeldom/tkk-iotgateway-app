// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/core/type.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <s7_micro_client.h>
#include <string>
#include <utility>
#include <vector>

namespace tkk::s7::limit {

// Following limits do relate to the latest S7 1518 plc, older plcs can still
// return out of bound errors etc. These are just sanity check limits

inline constexpr size_t kS7MinPea = 0;        // Siemens limit
inline constexpr size_t kS7MaxPea = 32767;    // Siemens limit
inline constexpr size_t kS7MinDb = 0;         // Siemens limit
inline constexpr size_t kS7MaxDb = 65535;     // Siemens limit
inline constexpr size_t kS7MinMk = 0;         // Siemens limit
inline constexpr size_t kS7MaxMk = 16383;     // Siemens limit
inline constexpr size_t kS7MinOffset = 0;     // Siemens limit
inline constexpr size_t kS7MaxOffset = 65535; // Siemens limit
inline constexpr size_t kS7MinBit = 0;        // Siemens limit
inline constexpr size_t kS7MaxBit = 7;        // Siemens limit
inline constexpr size_t kS7MinTm = 0;         // Siemens limit
inline constexpr size_t kS7MaxTm = 2047;      // Siemens limit
inline constexpr size_t kS7MinCt = 0;         // Siemens limit
inline constexpr size_t kS7MaxCt = 2047;      // Siemens limit
inline constexpr size_t kS7MinAmount = 1;     // Siemens limit
inline constexpr size_t kS7MaxAmount = 65536; // Siemens limit
inline constexpr size_t kS7MinPdu = 240;      // Siemens limit
inline constexpr size_t kS7MaxPdu = 960;      // Siemens limit
inline constexpr size_t kS7MinLength = 1;     // Practical config limit
inline constexpr size_t kS7MaxLength = 254;   // Practical config limit

inline constexpr size_t kMaxUInt = std::numeric_limits<int64_t>::max();
inline constexpr size_t kMaxNameLen = 128;
inline constexpr size_t kMinPollInterval = 250;
inline constexpr size_t kMinReconnDelay = 250;
inline constexpr size_t kMinReconnFactor = 2;

} // namespace tkk::s7::limit

namespace tkk::s7 {

namespace detail {
constexpr size_t kDefPollInterval = 5000;
constexpr size_t kDefInitReconnDelay = 2000;
constexpr size_t kDefMaxReconnDelay = 30000;
constexpr size_t kDefReconnFactor = 2;
} // namespace detail

enum class DataType : U8 {
  kUnset = 0,
  kBool,
  kByte,
  kWord,
  kDword,
  kSint,
  kInt,
  kDint,
  kUsint,
  kUint,
  kUdint,
  kReal,
  kLreal,
  kChar,
  kWchar,
  kString,
  kWstring,
  kS5Time,
  kTime,
  kDate,
  kTod,
  kTimer,
  kCounter,
};

enum class ReadMode : U8 {
  kUnset = 0,
  kMulti,
  kArea,
};

// Gets initialized with default values, so it doesnt block anything if the
// context could not be extracted from the config file yet (e.g. reconn delay =
// 0 would be a bad init value)
struct AdapterCtx {
  size_t id{0};
  std::string_view name;
  size_t poll_interval{detail::kDefPollInterval};
  size_t init_reconn_delay{detail::kDefInitReconnDelay};
  size_t max_reconn_delay{detail::kDefMaxReconnDelay};
  size_t reconn_factor{detail::kDefReconnFactor};
  bool enabled{false};
};

struct StateMetric {
  size_t reconnects{0};
  size_t retries{0};
  size_t reinits{0};
  size_t polls{0};
  size_t error_cnt{0};
  bool hard_fault{false};

  auto operator+=(const StateMetric &metric) -> StateMetric && {
    reconnects += metric.reconnects;
    retries += metric.retries;
    reinits += metric.reinits;
    polls += metric.polls;
    error_cnt += metric.error_cnt;
    hard_fault = metric.hard_fault;
    return std::move(*this);
  }
};

struct AdapterMetric {
  size_t tot_reconnects{0};
  size_t tot_retries{0};
  size_t tot_reinits{0};
  size_t tot_polls{0};
  size_t tot_error_cnt{0};
  bool started{false};
  bool hard_fault{false};

  auto operator+=(const StateMetric &metric) -> AdapterMetric && {
    tot_reconnects += metric.reconnects;
    tot_retries += metric.retries;
    tot_reinits += metric.reinits;
    tot_polls += metric.polls;
    tot_error_cnt += metric.error_cnt;
    hard_fault ? hard_fault : hard_fault = metric.hard_fault;
    return std::move(*this);
  }
};

struct TagItem {
  U32 id{0};
  std::string name;
  U8 target{0};
  I32 number{0};
  DataType type{DataType::kUnset};
  I32 offset{0};
  I32 length{0};
  U8 bit{0};
};

struct ReadHeader {
  ReadMode mode{ReadMode::kMulti};
  U8 target{0};
  U32 number{0};
  U32 offset{0};
  U8 status{0};

  U32 request_pdu_size{0};
  U32 response_pdu_size{0};
  U32 read_buffer_size{0};
  U32 item_count{0};
  U32 batch_no{0};

  std::chrono::system_clock::time_point timestamp;
};

struct S7Connection {
  std::string ip;
  I32 rack{0};
  I32 slot{2};
};

struct ReadBatch {
  ReadHeader header{};
  std::vector<TS7DataItem> snap7_data_items;
  std::vector<TagItem> tags;
  std::vector<U8> buffer;
  std::vector<DataPoint> data_points;

  // Delete copy assignment and constructor
  // Nessecary here, because TS7DataItem is holding pointers to a buffer
  // location noexcept tells a container that that the expression cannot "thorw"
  ReadBatch() = default;
  ReadBatch(const ReadBatch &) = delete;
  ReadBatch &operator=(const ReadBatch &) = delete;
  ReadBatch(ReadBatch &&) noexcept = default;
  ReadBatch &operator=(ReadBatch &&) noexcept = default;
  ~ReadBatch() = default;
};

using ReadBatchV = std::vector<ReadBatch>;

struct ConfigItem {
  AdapterCtx ctx;
  size_t total_dp{0};
  size_t total_batch{0};
  size_t total_read_size{0};
  ReadBatchV batches;
};
} // namespace tkk::s7