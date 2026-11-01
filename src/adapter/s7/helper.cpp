// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/helper.hpp"
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/type.hpp"
#include "tkk/core/util.hpp"
#include <algorithm>
#include <cassert>
#include <s7_types.h>
#include <string_view>

namespace tkk::s7::detail {
namespace {

struct DataTypeInfo {
  std::string_view key;
  DataType type{DataType::kUnset};
  U32 size{0};
};

constexpr std::array kDataTypeTable = {
    DataTypeInfo{.key = "BOOL", .type = DataType::kBool, .size = sizeof(U8)},
    DataTypeInfo{.key = "BYTE", .type = DataType::kByte, .size = sizeof(U8)},
    DataTypeInfo{.key = "WORD", .type = DataType::kWord, .size = sizeof(U16)},
    DataTypeInfo{.key = "DWORD", .type = DataType::kDword, .size = sizeof(U32)},
    DataTypeInfo{.key = "SINT", .type = DataType::kSint, .size = sizeof(U8)},
    DataTypeInfo{.key = "INT", .type = DataType::kInt, .size = sizeof(U16)},
    DataTypeInfo{.key = "DINT", .type = DataType::kDint, .size = sizeof(U32)},
    DataTypeInfo{.key = "USINT", .type = DataType::kUsint, .size = sizeof(U8)},
    DataTypeInfo{.key = "UINT", .type = DataType::kUint, .size = sizeof(U16)},
    DataTypeInfo{.key = "UDINT", .type = DataType::kUdint, .size = sizeof(U32)},
    DataTypeInfo{.key = "REAL", .type = DataType::kReal, .size = sizeof(F32)},
    DataTypeInfo{.key = "LREAL", .type = DataType::kLreal, .size = sizeof(F64)},
    DataTypeInfo{.key = "CHAR", .type = DataType::kChar, .size = sizeof(U8)},
    DataTypeInfo{.key = "WCHAR", .type = DataType::kWchar, .size = sizeof(U16)},
    DataTypeInfo{.key = "STRING", .type = DataType::kString, .size = sizeof(U16)},
    DataTypeInfo{.key = "WSTRING", .type = DataType::kWstring, .size = sizeof(U32)},
    DataTypeInfo{.key = "S5TIME", .type = DataType::kS5Time, .size = sizeof(U16)},
    DataTypeInfo{.key = "TIME", .type = DataType::kTime, .size = sizeof(U32)},
    DataTypeInfo{.key = "DATE", .type = DataType::kDate, .size = sizeof(U16)},
    DataTypeInfo{.key = "TOD", .type = DataType::kTod, .size = sizeof(U32)},
    DataTypeInfo{.key = "TIMER", .type = DataType::kTimer, .size = sizeof(U16)},
    DataTypeInfo{.key = "COUNTER", .type = DataType::kCounter, .size = sizeof(U16)},
};

struct Mode {
  std::string_view key;
  ReadMode mode;
};

constexpr std::array kModeTable = {
    Mode{.key = "area", .mode = ReadMode::kArea},
    Mode{.key = "multi", .mode = ReadMode::kMulti},
};

struct Target {
  std::string_view key;
  int s7_area;
};

constexpr std::array kTargetTable = {
    Target{.key = "e", .s7_area = S7AreaPE}, Target{.key = "a", .s7_area = S7AreaPA},
    Target{.key = "m", .s7_area = S7AreaMK}, Target{.key = "db", .s7_area = S7AreaDB},
    Target{.key = "c", .s7_area = S7AreaCT}, Target{.key = "t", .s7_area = S7AreaTM},
};
} // namespace

auto IsValidDataType(std::string_view type) -> bool {
  return std::ranges::contains(kDataTypeTable, type, &DataTypeInfo::key);
}

// Only use if data type validity was checked
auto StringToDataType(std::string_view type) -> DataType {
  const auto *it = std::ranges::find(kDataTypeTable, type, &DataTypeInfo::key);
  if (it == kDataTypeTable.end()) {
    assert(false);
    std::abort();
  }
  return it->type;
}

auto DataTypeToString(const DataType type) -> std::string_view {
  const auto *it = std::ranges::find(kDataTypeTable, type, &DataTypeInfo::type);
  if (it == kDataTypeTable.end()) {
    assert(false);
    std::abort();
  }
  return it->key;
}

auto IsValidReadMode(std::string_view mode) -> bool {
  return std::ranges::contains(kModeTable, mode, &Mode::key);
}

// Only use if read mode validity was checked
auto StringToReadMode(const std::string_view mode) -> ReadMode {
  const auto *it = std::ranges::find(kModeTable, mode, &Mode::key);
  if (it == kModeTable.end()) {
    assert(false);
    std::abort();
  }
  return it->mode;
}

auto IsValidS7Area(std::string_view target) -> bool {
  return std::ranges::contains(kTargetTable, target, &Target::key);
}

/**
 * @brief Convert target to Snap7 specific area code
 *
 * @param target_
 * @return int
 * @details This function returns the Snap7 specific 8bit code for area type
 */
// Only use if Snap7Area validity was checked
auto StringToSnap7Area(const std::string_view target) -> int {
  const auto *it = std::ranges::find(kTargetTable, target, &Target::key);
  if (it == kTargetTable.end()) {
    assert(false);
    std::abort();
  }
  return it->s7_area;
}

auto GetItemReadSize(const TagItem &item) -> U32 {
  const auto *it = std::ranges::find(kDataTypeTable, item.type, &DataTypeInfo::type);
  if (it == kDataTypeTable.end()) {
    assert(false);
    std::abort();
  }
  // Need length for strings
  if (item.type == DataType::kString) {
    return it->size + (sizeof(char8_t) * item.length); // 1 byte max length, 1 byte act length
  }
  if (item.type == DataType::kWstring) {
    return it->size + (sizeof(char16_t) * item.length); // 2 byte max length, 2 byte act length
  }
  return it->size;
}

/**
 * @brief Extract bytes from a byte buffer
 *
 * @param buffer
 * @param offset
 * @param amount
 * @return Result<SpanU8>
 */
auto ExtractBytes(SpanU8 buffer, U32 offset, U32 amount) -> Result<SpanU8> {

  if (offset + amount > buffer.size()) {
    return Error(ErrC::kDpError, ErrA::kFatal, "buffer overflow while byte extraction")
        .With("buffersize", buffer.size())
        .With("requested", offset + amount)
        .Fail();
  }
  return buffer.subspan(offset, amount);
}

auto SwapBytes(SpanU8 bytes, const TagItem &item) -> Result<Bytes> {
  using enum DataType;

  if (bytes.empty()) {
    return Error(ErrC::kDpError, ErrA::kFatal, "no data provided")
        .With("tag id", item.id)
        .With("requested", GetItemReadSize(item))
        .Fail();
  }
  if (bytes.size() != GetItemReadSize(item)) {
    return Error(ErrC::kDpError, ErrA::kFatal, "requested size does not match data type")
        .With("requested", bytes.size())
        .With("required", GetItemReadSize(item))
        .Fail();
  }

  switch (item.type) {
    // Signle bit
  case kBool: {
    if (static_cast<bool>((bytes[0] & (1 << item.bit)) != 0)) {
      return Bytes(1, 1);
    }
    return Bytes(1, 0);
  }
    // 1 byte
  case kByte:
  case kSint:
  case kUsint: {
    return Bytes(bytes.begin(), bytes.end());
  }
    // 2 byte
  case kWord:
  case kUint:
  case kDate:
  case kInt:
  case kCounter: {
    U16 raw{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    Bytes out(sizeof(raw));
    std::memcpy(out.data(), &raw, sizeof(raw));
    return out;
  }
    // 4 byte
  case kDint:
  case kTime:
  case kDword:
  case kUdint:
  case kTod:
  case kReal: {
    U32 raw{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    Bytes out(sizeof(raw));
    std::memcpy(out.data(), &raw, sizeof(raw));
    return out;
  }
  // 8 byte
  case kLreal: {
    U64 raw{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    Bytes out(sizeof(raw));
    std::memcpy(out.data(), &raw, sizeof(raw));
    return out;
  }
    // char
  case kChar: {
    const std::string text = util::Latin1ToUtf8(bytes);
    return Bytes(text.begin(), text.end());
  }
  // wchar
  case kWchar: {
    const std::string text = util::Utf16ToUtf8(bytes);
    return Bytes(text.begin(), text.end());
  }
  // string
  case kString: {
    // First byte: max length of string
    int max_length = std::min(item.length, static_cast<I32>(bytes[0]));
    // 2 byte offset for max/actual length of string
    std::string text = util::Latin1ToUtf8(bytes.subspan(2, max_length));
    return Bytes(text.begin(), text.end());
  }
  // wstring
  case kWstring: {
    U16 length{};
    // First 2 byte: max length of string
    std::memcpy(&length, bytes.data(), sizeof(length));
    length = std::byteswap(length);
    int max_length = std::min(item.length, static_cast<I32>(length));
    // 4 byte offset for max/actual length of string
    std::string text = util::Utf16ToUtf8(bytes.subspan(4, sizeof(char16_t) * max_length));
    return Bytes(text.begin(), text.end());
  }
  // Timer
  case kS5Time:
  case kTimer: {
    U16 raw{};

    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    U32 value = util::S5TimeToMillis(raw);
    Bytes out(sizeof(value));
    std::memcpy(out.data(), &value, sizeof(value));
    return out;
  }
  case kUnset:
    assert(false);
    break;
  }

  return Error(ErrC::kDpError, ErrA::kFatal, "fatal error while byte swapping")
      .With("tag id", item.id)
      .With("provided data size", bytes.size())
      .With("required", GetItemReadSize(item))
      .Fail();
}
} // namespace tkk::s7::detail