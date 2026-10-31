// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>
#include <variant>

namespace tkk {

using I8 = std::int8_t;
using U8 = std::uint8_t;
using I16 = std::int16_t;
using U16 = std::uint16_t;
using I32 = std::int32_t;
using U32 = std::uint32_t;
using I64 = std::int64_t;
using U64 = std::uint64_t;
using F32 = float;
using F64 = double;

using Json = nlohmann::json;

using GenericType = std::variant<std::string, bool, I8, U8, I16, U16, I32, U32, I64, U64, F32, F64>;

enum class Quality : U8 { kBad, kUncertain, kGood };

struct DataPoint {
  U32 id{0};
  Quality quality{Quality::kBad};
  GenericType data{0};
  std::string type;
  std::chrono::system_clock::time_point timestamp;
  std::string name;
};

class NoCopyNoMove {
protected:
  NoCopyNoMove() = default;
  ~NoCopyNoMove() = default;

public:
  NoCopyNoMove(const NoCopyNoMove &) = delete;
  NoCopyNoMove &operator=(const NoCopyNoMove &) = delete;
  NoCopyNoMove(NoCopyNoMove &&) = delete;
  NoCopyNoMove &operator=(NoCopyNoMove &&) = delete;
};

enum class ProtocolType : U8 {
  kUnset = 0,
  kS7,
  kOpcUa,
};

using UsetI = std::unordered_set<int>;
using UsetStr = std::unordered_set<std::string>;
using SpanU8 = std::span<const U8>;

enum class JsonType : U8 {
  kUnset = 0,
  kObject,
  kArray,
  kBool,
  kInt,
  kUInt,
  kString,
  kAny,
};

struct AdapterInfo {
  size_t id{};
  std::string name;
  ProtocolType protocol;
};
} // namespace tkk
