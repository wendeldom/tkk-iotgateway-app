// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/core/error.hpp"
#include <string_view>

namespace tkk::s7 {
[[nodiscard]] auto CheckRule(const Json &obj, std::string_view key) -> Result<void>;
[[nodiscard]] auto RequiredKey(const Json &obj, std::string_view key) -> Result<void>;
} // namespace tkk::s7