// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/error.hpp"
#include <cstddef>

namespace tkk::s7 {

class Creator final : private NoCopyNoMove {

public:
  Creator(const Json &config, int pdu_length) noexcept : config_{config}, pdu_length_{pdu_length} {}

  Creator(const Json &&, int) = delete;

  [[nodiscard]] auto Create() -> Result<ConfigItem>;

private:
  auto CreateAdapterCtx() -> void;
  [[nodiscard]] auto CreateReadConfig() -> Result<void>;
  auto SetupBatches() -> void;
  auto SetupS7Items() -> void;

  const Json &config_;
  const int pdu_length_;
  ConfigItem config_item_{};
};

class AreaCreator final : private NoCopyNoMove {

public:
  AreaCreator(const Json &config, ReadBatchV &batches) noexcept
      : config_{config}, batches_{batches} {}

  AreaCreator(const Json &&, ReadBatchV &) = delete;

  [[nodiscard]] auto Create() -> Result<void>;

private:
  [[nodiscard]] static auto CreateTagItem(const Json &tag) -> TagItem;

  const Json &config_;
  ReadBatchV &batches_;
};

class MultiCreator final : private NoCopyNoMove {

public:
  MultiCreator(const Json &config, ReadBatchV &batches, size_t pdu_length) noexcept
      : config_{config}, batches_{batches}, pdu_length_{pdu_length} {}

  MultiCreator(const Json &&, ReadBatchV, size_t) = delete;

  [[nodiscard]] auto Create() -> Result<void>;

private:
  [[nodiscard]] static auto CreateTagItem(const Json &tag) -> TagItem;

  const Json &config_;
  ReadBatchV &batches_;
  const size_t pdu_length_;
};
} // namespace tkk::s7
