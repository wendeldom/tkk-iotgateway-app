// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/error.hpp"

namespace tkk::s7 {

class IAdapter {
public:
  virtual ~IAdapter() = default;
  IAdapter() = default;
  IAdapter(const IAdapter &) = delete;
  auto operator=(const IAdapter &) -> IAdapter & = delete;
  IAdapter(IAdapter &&) = delete;
  auto operator=(IAdapter &&) -> IAdapter & = delete;

  [[nodiscard]] virtual auto Init() -> Result<void> = 0;
  [[nodiscard]] virtual auto Connect() -> Result<void> = 0;
  [[nodiscard]] virtual auto Reconnect() -> Result<void> = 0;
  [[nodiscard]] virtual auto Prepare() -> Result<void> = 0;
  [[nodiscard]] virtual auto Poll() -> Result<void> = 0;
  [[nodiscard]] virtual auto GetContext() const -> const AdapterCtx & = 0;
  virtual auto PublishStateMetric(StateMetric) -> void = 0;
};

} // namespace tkk::s7
