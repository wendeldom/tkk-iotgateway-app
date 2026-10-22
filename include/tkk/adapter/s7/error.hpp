// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/core/error.hpp"
#include <string_view>

namespace tkk::s7::error {

struct SnapErrInfo {
  std::string_view text;
  ErrA action{ErrA::kFatal};
};

[[nodiscard]] auto GetSnapErrInfo(int err) -> SnapErrInfo;
} // namespace tkk::s7::error