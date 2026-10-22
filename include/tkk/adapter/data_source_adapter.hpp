// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/adapter/s7/type.hpp"

namespace tkk {
/**
 * @brief Interface for communication adapters
 * @warning All methods are non thread safe
 */
struct IDataSourceAdapter {
public:
  virtual ~IDataSourceAdapter() = default;
  IDataSourceAdapter() = default;
  IDataSourceAdapter(const IDataSourceAdapter &) = delete;
  auto operator=(const IDataSourceAdapter &) -> IDataSourceAdapter & = delete;
  IDataSourceAdapter(IDataSourceAdapter &&) = delete;
  auto operator=(IDataSourceAdapter &&) -> IDataSourceAdapter & = delete;

  virtual void Start() = 0;
  [[nodiscard]] virtual auto GetAdapterMetric() -> s7::AdapterMetric = 0;
};
} // namespace tkk