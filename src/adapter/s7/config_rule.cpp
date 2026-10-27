// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/config_rule.hpp"
#include "tkk/adapter/s7/config_key.hpp"
#include "tkk/adapter/s7/helper.hpp"
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/util.hpp"
#include <s7_types.h>
#include <span>
#include <string_view>

namespace key = tkk::s7::detail::key;

namespace tkk::s7 {

namespace {
using ValidatorPtr = Result<void> (*)(const Json &obj, std::string_view key);

[[nodiscard]] auto CheckRange(const Json &obj, std::string_view key) -> Result<void>;
[[nodiscard]] auto CheckProtocol(const Json &obj, std::string_view key) -> Result<void>;
[[nodiscard]] auto CheckMode(const Json &obj, std::string_view key) -> Result<void>;
[[nodiscard]] auto CheckIp(const Json &obj, std::string_view key) -> Result<void>;
[[nodiscard]] auto CheckTarget(const Json &obj, std::string_view key) -> Result<void>;
[[nodiscard]] auto CheckNumber(const Json &obj, std::string_view key) -> Result<void>;
[[nodiscard]] auto CheckDataType(const Json &obj, std::string_view key) -> Result<void>;
[[nodiscard]] auto CheckAreaTarget(const Json &obj, std::string_view key) -> Result<void>;
[[nodiscard]] auto GetKeyType(std::string_view key) -> JsonType;
[[nodiscard]] auto IsValidNumber(int target, int value) -> bool;
[[nodiscard]] auto JsonTypeToString(JsonType type) -> std::string_view;

struct Rule {
  std::string_view key;
  ValidatorPtr check = nullptr;
};

struct TargetDep {
  int target; // Snap7 specific area types
  std::span<const std::string_view> dep_keys;
};

struct DataTypeDep {
  DataType type;
  std::string_view dep_key{detail::key::kDefault};
};

struct KeyType {
  std::string_view key;
  JsonType type;
};

struct KeyRange {
  std::string_view key;
  size_t min{0};
  size_t max{0};
};

// Required keys for target = db
constexpr std::array kDbDepKeys{
    detail::key::kNumber,
    detail::key::kOffset,
    detail::key::kDataType,
};
// Required keys for target = e,a,m
constexpr std::array kEamDepKeys{
    detail::key::kOffset,
    detail::key::kDataType,
};
// Required keys for target = timer/counter
constexpr std::array kTcDepKeys{
    detail::key::kNumber,
};
// Return the json type for a given key
constexpr std::array kEyTypes = {
    KeyType{.key = detail::key::kAdapter, .type = JsonType::kObject},
    KeyType{.key = detail::key::kConnection, .type = JsonType::kObject},
    KeyType{.key = detail::key::kPolling, .type = JsonType::kObject},
    KeyType{.key = detail::key::kReconnect, .type = JsonType::kObject},
    KeyType{.key = detail::key::kRead, .type = JsonType::kArray},
    KeyType{.key = detail::key::kAreaParams, .type = JsonType::kObject},
    KeyType{.key = detail::key::kDataType, .type = JsonType::kString},
    KeyType{.key = detail::key::kProtocol, .type = JsonType::kString},
    KeyType{.key = detail::key::kId, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kName, .type = JsonType::kString},
    KeyType{.key = detail::key::kEnabled, .type = JsonType::kBool},
    KeyType{.key = detail::key::kIp, .type = JsonType::kString},
    KeyType{.key = detail::key::kRack, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kSlot, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kIntervalMs, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kInitialDelayMs, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kMaxDelayMs, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kFactor, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kMode, .type = JsonType::kString},
    KeyType{.key = detail::key::kTags, .type = JsonType::kArray},
    KeyType{.key = detail::key::kTarget, .type = JsonType::kString},
    KeyType{.key = detail::key::kAreaTarget, .type = JsonType::kString},
    KeyType{.key = detail::key::kNumber, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kOffset, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kBit, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kAmount, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kLength, .type = JsonType::kUInt},
    KeyType{.key = detail::key::kDefault, .type = JsonType::kAny},
};
// Return the ranges for a given key
constexpr std::array kKeyRanges = {
    KeyRange{.key = detail::key::kId, .min = 0, .max = limit::kMaxUInt},
    KeyRange{.key = detail::key::kName, .min = 0, .max = limit::kMaxNameLen},
    KeyRange{.key = detail::key::kRack, .min = 0, .max = limit::kMaxUInt},
    KeyRange{.key = detail::key::kSlot, .min = 0, .max = limit::kMaxUInt},
    KeyRange{
        .key = detail::key::kIntervalMs, .min = limit::kMinPollInterval, .max = limit::kMaxUInt},
    KeyRange{
        .key = detail::key::kInitialDelayMs, .min = limit::kMinReconnDelay, .max = limit::kMaxUInt},
    KeyRange{
        .key = detail::key::kMaxDelayMs, .min = limit::kMinReconnDelay, .max = limit::kMaxUInt},
    KeyRange{.key = detail::key::kFactor, .min = limit::kMinReconnFactor, .max = limit::kMaxUInt},
    KeyRange{.key = detail::key::kOffset, .min = limit::kS7MinOffset, .max = limit::kS7MaxOffset},
    KeyRange{.key = detail::key::kBit, .min = limit::kS7MinBit, .max = limit::kS7MaxBit},
    KeyRange{.key = detail::key::kAmount, .min = limit::kS7MinAmount, .max = limit::kS7MaxAmount},
    KeyRange{.key = detail::key::kLength, .min = limit::kS7MinLength, .max = limit::kS7MaxLength},
};
// Return the keys required for a given target type
constexpr std::array kTargetDep = {
    TargetDep{.target = S7AreaPE, .dep_keys = kEamDepKeys},
    TargetDep{.target = S7AreaPA, .dep_keys = kEamDepKeys},
    TargetDep{.target = S7AreaMK, .dep_keys = kEamDepKeys},
    TargetDep{.target = S7AreaDB, .dep_keys = kDbDepKeys},
    TargetDep{.target = S7AreaTM, .dep_keys = kTcDepKeys},
    TargetDep{.target = S7AreaCT, .dep_keys = kTcDepKeys},
};
// Return the keys required for a given data type
constexpr std::array kDataTypeDep = {
    DataTypeDep{.type = DataType::kString, .dep_key = detail::key::kLength},
    DataTypeDep{.type = DataType::kWstring, .dep_key = detail::key::kLength},
    DataTypeDep{.type = DataType::kBool, .dep_key = detail::key::kBit},
};
constexpr std::array kRules{
    Rule{.key = detail::key::kDataType, .check = CheckDataType},
    Rule{.key = detail::key::kProtocol, .check = CheckProtocol},
    Rule{.key = detail::key::kId, .check = CheckRange},
    Rule{.key = detail::key::kName, .check = CheckRange},
    Rule{.key = detail::key::kIp, .check = CheckIp},
    Rule{.key = detail::key::kRack, .check = CheckRange},
    Rule{.key = detail::key::kSlot, .check = CheckRange},
    Rule{.key = detail::key::kIntervalMs, .check = CheckRange},
    Rule{.key = detail::key::kInitialDelayMs, .check = CheckRange},
    Rule{.key = detail::key::kMaxDelayMs, .check = CheckRange},
    Rule{.key = detail::key::kFactor, .check = CheckRange},
    Rule{.key = detail::key::kMode, .check = CheckMode},
    Rule{.key = detail::key::kTarget, .check = CheckTarget},
    Rule{.key = detail::key::kAreaTarget, .check = CheckAreaTarget},
    Rule{.key = detail::key::kNumber, .check = CheckNumber},
    Rule{.key = detail::key::kOffset, .check = CheckRange},
    Rule{.key = detail::key::kAmount, .check = CheckRange},
    Rule{.key = detail::key::kBit, .check = CheckRange},
    Rule{.key = detail::key::kLength, .check = CheckRange},
};

auto CheckRange(const Json &obj, const std::string_view key) -> Result<void> {

  const auto *it = std::ranges::find(kKeyRanges, key, &KeyRange::key);
  if (it == kKeyRanges.end()) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "key ranges missing")
        .WithStr("key", key)
        .Fail();
  }

  auto type = GetKeyType(key);

  switch (type) {
  case JsonType::kUInt:
    if (util::InRange(util::GetUint(obj, key), it->min, it->max)) {
      return {};
    }
    break;
  case JsonType::kInt:
    if (util::InRange(util::GetInt(obj, key), it->min, it->max)) {
      return {};
    }
    break;
  case JsonType::kString:
    if (util::InRange(util::GetStr(obj, key).length(), it->min, it->max)) {
      return {};
    }
    break;
  default:
    return {};
  }
  return Error(ErrC::kConfigInvalid, ErrA::kFatal, "key value out of bounds")
      .WithStr("key", key)
      .With("min", it->min)
      .With("max", it->max)
      .Fail();
}

auto CheckProtocol(const Json &obj, const std::string_view key) -> Result<void> {
  using util::GetStr;

  if (!util::IsValidProtocol(util::GetStr(obj, key))) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "unknown protocol")
        .WithStr("at", GetStr(obj, key))
        .Fail();
  }
  return {};
}

auto CheckDataType(const Json &obj, const std::string_view key) -> Result<void> {

  if (!detail::IsValidDataType(util::GetStr(obj, key))) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "invalid data type")
        .WithStr("at", util::GetStr(obj, key))
        .With("at id", util::GetUint(obj, key::kId))
        .Fail();
  }
  const auto data_type = detail::StringToDataType(util::GetStr(obj, key));

  // Check dataType dependent keys
  const auto *it = std::ranges::find(kDataTypeDep, data_type, &DataTypeDep::type);
  if (it == kDataTypeDep.end()) {
    return {};
  }
  const auto res = RequiredKey(obj, it->dep_key);
  if (!res) {
    return res;
  }

  return {};
}

auto CheckAreaTarget(const Json &obj, const std::string_view key) -> Result<void> {
  if (!detail::IsValidS7Area(util::GetStr(obj, key))) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "invalid area target")
        .WithStr("at", util::GetStr(obj, key))
        .Fail();
  }

  const auto target = detail::StringToSnap7Area(util::GetStr(obj, key));
  if (target == S7AreaDB) {
    return RequiredKey(obj, detail::key::kNumber);
  }

  return {};
}

auto CheckIp(const Json &obj, const std::string_view key) -> Result<void> {

  const auto ip = util::IsValidIp(util::GetStr(obj, key));

  if (!ip) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "invalid ip").Fail();
  }
  return {};
}

auto CheckTarget(const Json &obj, const std::string_view key) -> Result<void> {
  if (!detail::IsValidS7Area(util::GetStr(obj, key))) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "invalid target")
        .WithStr("at", util::GetStr(obj, key))
        .With("at id", util::GetUint(obj, key::kId))
        .Fail();
  }

  const auto target = detail::StringToSnap7Area(util::GetStr(obj, key));

  // Check target dependent keys
  const auto *it = std::ranges::find(kTargetDep, target, &TargetDep::target);
  if (it == kTargetDep.end()) {
    return {};
  }

  for (const auto key : it->dep_keys) {
    if (auto res = RequiredKey(obj, key); !res) {
      return res;
    }
    if (auto res = CheckRule(obj, key); !res) {
      return res;
    }
  }

  return {};
}

auto CheckNumber(const Json &obj, const std::string_view key) -> Result<void> {

  // Get 'target' value because limits depend on it
  int target = detail::StringToSnap7Area(util::GetStr(obj, detail::key::kTarget));
  int value = util::GetInt(obj, key);

  if (!IsValidNumber(target, value)) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "invalid number")
        .With("at", value)
        .With("at id", util::GetUint(obj, key::kId))
        .Fail();
  }
  return {};
}

auto CheckMode(const Json &obj, const std::string_view key) -> Result<void> {

  if (!detail::IsValidReadMode(util::GetStr(obj, key))) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "invalid read mode")
        .WithStr("at", util::GetStr(obj, key))
        .Fail();
  }
  return {};
}

auto IsValidNumber(int target, int value) -> bool {

  // Check only the relevant targets for 'number' key which are DB, Timers,
  // Counters. Other targets dont need the 'number' field anyways
  switch (target) {
  case S7AreaPE:
  case S7AreaPA:
  case S7AreaMK:
    return true;
  case S7AreaDB:
    return util::InRange(value, limit::kS7MinDb, limit::kS7MaxDb);
  case S7AreaTM:
    return util::InRange(value, limit::kS7MinTm, limit::kS7MaxTm);
  case S7AreaCT:
    return util::InRange(value, limit::kS7MinCt, limit::kS7MaxCt);
  default:
    return false;
  }
  return false;
}

auto GetKeyType(const std::string_view key) -> JsonType {
  using enum JsonType;
  const auto *it = std::ranges::find(kEyTypes, key, &KeyType::key);
  if (it == kEyTypes.end()) {
    assert(false);
    std::abort();
  }
  return it->type;
}

auto JsonTypeToString(JsonType type) -> std::string_view {
  using enum JsonType;
  switch (type) {
  case kAny:
    return "any";
  case kObject:
    return "object";
  case kArray:
    return "array";
  case kBool:
    return "bool";
  case kInt:
    return "int";
  case kUInt:
    return "uint";
  case kString:
    return "string";
  case kUnset:
    break;
  }
  return "invalid";
}
} // namespace

auto RequiredKey(const Json &obj, const std::string_view key) -> Result<void> {
  using enum JsonType;

  bool type_fit{false};

  auto key_val = obj.find(key);
  if (key_val == obj.end()) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "missing key").WithView("key", key).Fail();
  }
  auto type = GetKeyType(key);

  switch (type) {
  case kAny:
    type_fit = true;
    break;
  case kObject:
    key_val->is_object() ? type_fit = true : type_fit = false;
    break;
  case kArray:
    key_val->is_array() ? type_fit = true : type_fit = false;
    break;
  case kBool:
    key_val->is_boolean() ? type_fit = true : type_fit = false;
    break;
  case kInt:
    key_val->is_number_integer() ? type_fit = true : type_fit = false;
    break;
  case kUInt:
    key_val->is_number_unsigned() ? type_fit = true : type_fit = false;
    break;
  case kString:
    key_val->is_string() ? type_fit = true : type_fit = false;
    break;
  case kUnset:
    assert(false);
    std::abort();
  }
  if (!type_fit) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "type mismatch")
        .WithView("key", key)
        .WithView("required", JsonTypeToString(type))
        .Fail();
  }
  return {};
}

auto CheckRule(const Json &obj, const std::string_view key) -> Result<void> {
  const auto *it = std::ranges::find(kRules, key, &Rule::key);
  if (it == kRules.end()) {
    return {};
  }
  return it->check(obj, key);
}

} // namespace tkk::s7