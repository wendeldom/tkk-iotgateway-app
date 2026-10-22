// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <filesystem>

namespace tkk::s7 {

class Validator final : private NoCopyNoMove {
public:
  explicit Validator(Json &&config) : config_(std::move(config)) {}

  [[nodiscard]] auto Validate() -> Result<Json>;

private:
  [[nodiscard]] auto ValidateScheme() -> Result<void>;

  std::filesystem::path path_;
  ProtocolType protocol_{ProtocolType::kUnset};
  const Json config_;
};

class CommonValidator final : private NoCopyNoMove {
public:
  explicit CommonValidator(const Json &config) : config_{config} {}

  // Delete constructor for Rvalue Reference
  CommonValidator(const Json &&) = delete;

  [[nodiscard]] auto Validate() -> Result<void>;

private:
  [[nodiscard]] auto ValidateAdapter() -> Result<void>;
  [[nodiscard]] auto ValidateConnection() -> Result<void>;
  [[nodiscard]] auto ValidatePolling() -> Result<void>;
  [[nodiscard]] auto ValidateReconnect() -> Result<void>;
  [[nodiscard]] auto ValidateRead() -> Result<void>;
  [[nodiscard]] auto ValidateReadItem(const Json &config) -> Result<void>;
  [[nodiscard]] auto CheckId(int id) -> bool;

  const Json &config_;
  UsetI seen_ids_;
  friend class AreaValidator;
  friend class MultiValidator;
};

class AreaValidator final : private NoCopyNoMove {
public:
  AreaValidator(const Json &config, CommonValidator &common_val)
      : config_{config}, common_val_{common_val} {}

  // Delete constructor for Rvalue Reference
  AreaValidator(const Json &&) = delete;

  [[nodiscard]] auto Validate() -> Result<void>;

private:
  [[nodiscard]] auto ValidateParams() -> Result<void>;
  [[nodiscard]] auto ValidateTags() -> Result<void>;

  const Json &config_;
  CommonValidator &common_val_;
};

class MultiValidator final : private NoCopyNoMove {
public:
  explicit MultiValidator(const Json &config, CommonValidator &common_val)
      : config_{config}, common_val_{common_val} {}

  // Delete constructor for Rvalue Reference
  MultiValidator(const Json &&) = delete;

  [[nodiscard]] auto Validate() -> Result<void>;

private:
  [[nodiscard]] auto ValidateTags() -> Result<void>;

  const Json &config_;
  CommonValidator &common_val_;
};

[[nodiscard]] auto GetConnParams(const Json &config) -> S7Connection;
[[nodiscard]] auto GetAdapterParams(const Json &config) -> Result<AdapterInfo>;
} // namespace tkk::s7
