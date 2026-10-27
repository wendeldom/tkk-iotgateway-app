// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/helper.hpp"
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/util.hpp"
#include <algorithm>
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

/**
 * @brief Convert bytes to dataType
 *
 * @param bytes
 * @param item
 * @return Result<GenericType>
 */
auto CvrtBytesToType(SpanU8 bytes, const TagItem &item) -> Result<GenericType> {
  using enum DataType;

  if (bytes.size() != GetItemReadSize(item)) {
    return Error(ErrC::kDpError, ErrA::kFatal, "requested size does not match data type")
        .With("requested", bytes.size())
        .With("required", GetItemReadSize(item))
        .Fail();
  }

  switch (item.type) {
    // Signle bit
  case kBool: {
    U8 raw{};
    raw = bytes[0];
    return static_cast<bool>((raw & (1 << item.bit)) != 0);
  }
    // Unsigned 1 byte
  case kByte:
  case kUsint: {
    U8 value{};
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
  }
    // Signed 1 byte
  case kSint: {
    I8 value{};
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
  }
    // Signed 2 byte
  case kInt: {
    I16 raw{};
    I16 value{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    std::memcpy(&value, &raw, sizeof(value));
    return value;
  }
    // Signed 4 byte
  case kDint:
  case kTime: {
    I32 raw{};
    I32 value{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    std::memcpy(&value, &raw, sizeof(value));
    return value;
  }
    // Unsigned 2 byte
  case kWord:
  case kUint:
  case kDate: {
    U16 raw{};
    U16 value{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    std::memcpy(&value, &raw, sizeof(value));
    return value;
  }
    // Unsigned 4 byte
  case kDword:
  case kUdint:
  case kTod: {
    U32 raw{};
    U32 value{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    std::memcpy(&value, &raw, sizeof(value));
    return value;
  }
  // IEEE Standard 4 byte float
  case kReal: {
    U32 raw{};
    F32 value{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    std::memcpy(&value, &raw, sizeof(value));
    return value;
  }
  // IEEE Standard 8 byte double
  // Date and Time
  case kLreal: {
    U64 raw{};
    F64 value{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    std::memcpy(&value, &raw, sizeof(value));
    return value;
  }
    // String
  case kChar: {
    std::string value{};
    value = util::Latin1ToUtf8(bytes);
    return value;
  }
  case kWchar: {
    std::string value{};
    value = util::Utf16ToUtf8(bytes);
    return value;
  }
  case kString: {
    std::string value{};

    int max_length = std::min(item.length,
                              static_cast<I32>(bytes[0]));    // First byte: max length of string
    value = util::Latin1ToUtf8(bytes.subspan(2, max_length)); // Offst 2 byte (max/actual len)
    return value;
  }
  case kWstring: {
    std::string value{};
    U16 len{};

    std::memcpy(&len, bytes.data(),
                sizeof(len)); // First 2 byte: max length of string
    len = std::byteswap(len);
    int max_length = std::min(item.length, static_cast<I32>(len));
    value = util::Latin1ToUtf8(
        bytes.subspan(4, sizeof(char16_t) * max_length)); // Offset 4 byte (max/actual len)
    return value;
  }
  // Timer
  case kS5Time:
  case kTimer: {
    U16 raw{};
    U16 value{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    std::memcpy(&value, &raw, sizeof(value));
    return util::S5TimeToMillis(value);
  }
  // Counter
  case kCounter: {
    U16 raw{};
    U16 value{};
    std::memcpy(&raw, bytes.data(), sizeof(raw));
    raw = std::byteswap(raw);
    std::memcpy(&value, &raw, sizeof(value));
    return value;
  }
  case kUnset:
    assert(false);
    break;
  }
  return Error(ErrC::kDpError, ErrA::kFatal, "Data type could not be converted!")
      .WithStr("requested", DataTypeToString(item.type))
      .Fail();
}

} // namespace tkk::s7::detail