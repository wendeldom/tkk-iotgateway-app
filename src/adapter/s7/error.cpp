// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/error.hpp"
#include <cassert>
#include <s7_micro_client.h>

namespace tkk::s7::error {

auto GetSnapErrInfo(const int err) -> SnapErrInfo {

  // Snap7 sepcific error masks
  constexpr U32 kTcpErrMsk = 0x0000FFFF;
  constexpr U32 kIsoErrMsk = 0x000F0000;
  constexpr U32 kClientErrMsk = 0xFFF00000;

  if (const auto error = err & kTcpErrMsk; error != 0) {
    switch (error) {
    case WSAETIMEDOUT:
      return {.text = "tcp: connection timeout", .action = ErrA::kReconnect};
    case WSAECONNREFUSED:
      return {.text = "tcp: connection refused", .action = ErrA::kReconnect};
    case WSAEHOSTDOWN:
      return {.text = "tcp: host down", .action = ErrA::kReconnect};
    case WSAEHOSTUNREACH:
      return {.text = "tcp: host unreachable", .action = ErrA::kReconnect};
    default:
      break;
    }
  }
  if (const auto error = err & kIsoErrMsk; error != 0) {
    switch (error) {
    case errIsoConnect:
      return {.text = "iso: connection error", .action = ErrA::kReconnect};
    case errIsoSendPacket:
      return {.text = "iso: send error", .action = ErrA::kReconnect};
    case errIsoRecvPacket:
      return {.text = "iso: receive error", .action = ErrA::kReconnect};
    default:
      break;
    }
  }
  if (const auto error = err & kClientErrMsk; error != 0) {
    switch (error) {
    case errCliSizeOverPDU:
      return {.text = "client: size over pdu", .action = ErrA::kReconnect};
    case errCliInvalidPlcAnswer:
      return {.text = "client: invalid plc answer", .action = ErrA::kReconnect};
    case errCliAddressOutOfRange:
      return {.text = "client: address out of range", .action = ErrA::kSkip};
    case errCliItemNotAvailable:
      return {.text = "client: item not available", .action = ErrA::kSkip};
    case errCliJobTimeout:
      return {.text = "client: job timeout", .action = ErrA::kReconnect};
    default:
      break;
    }
  }
  return {.text = "unknown", .action = ErrA::kReconnect};
}
} // namespace tkk::s7::error