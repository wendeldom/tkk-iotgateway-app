// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <string_view>

namespace tkk::s7::detail::key {
inline constexpr std::string_view kAdapter = "adapter";
inline constexpr std::string_view kConnection = "connection";
inline constexpr std::string_view kPolling = "polling";
inline constexpr std::string_view kReconnect = "reconnect";
inline constexpr std::string_view kRead = "read";
inline constexpr std::string_view kAreaParams = "areaParams";
inline constexpr std::string_view kAreaTarget = "areaTarget";
inline constexpr std::string_view kDataType = "dataType";
inline constexpr std::string_view kProtocol = "protocol";
inline constexpr std::string_view kId = "id";
inline constexpr std::string_view kName = "name";
inline constexpr std::string_view kEnabled = "enabled";
inline constexpr std::string_view kIp = "ip";
inline constexpr std::string_view kRack = "rack";
inline constexpr std::string_view kSlot = "slot";
inline constexpr std::string_view kIntervalMs = "intervalMs";
inline constexpr std::string_view kInitialDelayMs = "initialDelayMs";
inline constexpr std::string_view kMaxDelayMs = "maxDelayMs";
inline constexpr std::string_view kFactor = "factor";
inline constexpr std::string_view kMode = "mode";
inline constexpr std::string_view kTags = "tags";
inline constexpr std::string_view kTarget = "target";
inline constexpr std::string_view kNumber = "number";
inline constexpr std::string_view kOffset = "offset";
inline constexpr std::string_view kBit = "bit";
inline constexpr std::string_view kAmount = "amount";
inline constexpr std::string_view kLength = "length";
inline constexpr std::string_view kDefault = "DEFAULT_KEY";
} // namespace tkk::s7::detail::key
