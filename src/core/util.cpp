// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/core/util.hpp"
#include "tkk/core/error.hpp"
#include <arpa/inet.h>
#include <format>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
// #include <print>
#include <utf8/checked.h>

namespace tkk::util {

namespace {
struct Protocol {
  std::string_view key;
  ProtocolType type;
};

constexpr std::array kProtocolTable = {
    Protocol{.key = "s7", .type = ProtocolType::kS7},
    Protocol{.key = "opcua", .type = ProtocolType::kOpcUa},
};

} // namespace

auto IsValidProtocol(std::string_view prot) -> bool {
  return std::ranges::contains(kProtocolTable, prot, &Protocol::key);
}

// Only use if protocol validity was checked
auto StringToProtocol(const std::string_view prot) -> ProtocolType {
  const auto *it = std::ranges::find(kProtocolTable, prot, &Protocol::key);
  if (it == kProtocolTable.end()) {
    assert(false);
    std::abort();
  }
  return it->type;
}

auto IsValidIp(const std::string_view ip) -> bool {

  struct in_addr addr{};
  auto res = inet_pton(AF_INET, ip.begin(), &addr);
  return !(res != 1);
}

/**
 * @brief Convert s5time format to miliseconds
 *
 * @param time_
 * @return U32
 */
auto S5TimeToMillis(const U16 time) -> U32 {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
  // Bit 12/13 give the time base
  U8 base = (time >> 12);
  base &= 0x03;
  // Goddamn this s5time format
  // bit 0-3: decimal but only valid for 0-9 * 1
  // bit 4-7: decimal but only valid for 0-9 * 10
  // bit 8-11: decimal but only valid for 0-9 * 100

  // bit 0-3
  U8 bcd1 = (time);
  bcd1 &= 0x0F; // Lower 4 bit
  // bit 4-7
  U8 bcd2 = (time >> 4);
  bcd2 &= 0x0F; // Lower 4 bit
  // bit 8-11
  U8 bcd3 = (time >> 8);
  bcd3 &= 0x0F; // Lower 4 bit
  U16 bcd_value = (bcd3 * 100) + (bcd2 * 10) + (bcd1 * 1);

  U32 bcd_miliseconds{};
  switch (base) {
  case 0:
    bcd_miliseconds = bcd_value * 10;
    break;
  case 1:
    bcd_miliseconds = bcd_value * 100;
    break;
  case 2:
    bcd_miliseconds = bcd_value * 1000;
    break;
  case 3:
    bcd_miliseconds = bcd_value * 10000;
    break;
  default:
    bcd_miliseconds = bcd_value * 1000;
    break;
  }
  return bcd_miliseconds;
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

auto Latin1ToUtf8(SpanU8 bytes) -> std::string {
  std::string result;
  for (auto byte : bytes) {
    utf8::append(static_cast<char32_t>(byte), result);
  }
  return result;
}

auto Utf16ToUtf8(SpanU8 bytes) -> std::string {
  std::u16string u16;
  for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    u16.push_back(static_cast<char16_t>(bytes[i] << 8 | bytes[i + 1]));
  }
  std::string result;
  utf8::utf16to8(u16.begin(), u16.end(), std::back_inserter(result));
  return result;
}
} // namespace tkk::util

namespace tkk::log {
void LogErrors(std::vector<Error> &&errs, std::string_view source) {
  static std::mutex log_mtx;
  std::scoped_lock lock(log_mtx);
  for (const auto &error : std::move(errs)) {
    std::cerr << std::format("[src: {}] {}", source, error.Format()) << "\n";
  }
}
} // namespace tkk::log