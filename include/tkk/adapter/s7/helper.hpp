// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/error.hpp"
#include <string_view>

namespace tkk::s7::detail {

[[nodiscard]] auto IsValidDataType(std::string_view type) -> bool;
[[nodiscard]] auto StringToDataType(std::string_view type) -> DataType;
[[nodiscard]] auto DataTypeToString(DataType type) -> std::string_view;
[[nodiscard]] auto IsValidReadMode(std::string_view mode) -> bool;
[[nodiscard]] auto StringToReadMode(std::string_view mode) -> ReadMode;
[[nodiscard]] auto IsValidS7Area(std::string_view target) -> bool;
[[nodiscard]] auto StringToSnap7Area(std::string_view target) -> int;
[[nodiscard]] auto GetItemReadSize(const TagItem &item) -> U32;
[[nodiscard]] auto ExtractBytes(SpanU8 buffer, U32 offset, U32 amount) -> Result<SpanU8>;
[[nodiscard]] auto SwapBytes(SpanU8 bytes, const TagItem &item) -> Result<Bytes>;

} // namespace tkk::s7::detail
