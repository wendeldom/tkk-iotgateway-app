// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <string_view>

namespace tkk::util {
// Getter functions for json keys
template <typename T> auto Get(const Json &obj, std::string_view key) -> T {
  return obj.at(key).get<T>();
}

inline auto GetUint(const Json &obj, std::string_view key) -> size_t {
  return Get<size_t>(obj, key);
}

inline auto GetInt(const Json &obj, std::string_view key) -> int {
  return Get<int>(obj, key);
}

inline auto GetStr(const Json &obj, std::string_view key) -> std::string {
  return Get<std::string>(obj, key);
}

[[nodiscard]] auto IsValidProtocol(std::string_view prot) -> bool;
[[nodiscard]] auto StringToProtocol(std::string_view prot) -> ProtocolType;
[[nodiscard]] auto IsValidIp(std::string_view ip) -> bool;
[[nodiscard]] auto S5TimeToMillis(U16 time) -> U32;
[[nodiscard]] auto Latin1ToUtf8(SpanU8 bytes) -> std::string;
[[nodiscard]] auto Utf16ToUtf8(SpanU8 bytes) -> std::string;

template <std::integral T, std::integral A, std::integral B>
[[nodiscard]] constexpr auto InRange(T val, A min, B max) noexcept -> bool {
  return std::cmp_greater_equal(val, min) && std::cmp_less_equal(val, max);
}

} // namespace tkk::util

namespace tkk::log {
void LogErrors(std::vector<Error> &&errs, std::string_view source);
}