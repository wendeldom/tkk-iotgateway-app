// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/adapter/data_source_adapter.hpp"
#include "tkk/adapter/s7/client.hpp"
#include "tkk/adapter/s7/state.hpp"
#include "tkk/core/error.hpp"
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

namespace tkk::s7 {

class Adapter final : public IDataSourceAdapter, public IAdapter {

public:
  Adapter(Json &&config, size_t id, std::string name,
          std::unique_ptr<ISnap7Client> client = nullptr, std::unique_ptr<State> state = nullptr);
  ~Adapter();

private:
  // Context interface
  [[nodiscard]] auto Init() -> Result<void> override;
  [[nodiscard]] auto Connect() -> Result<void> override;
  [[nodiscard]] auto Reconnect() -> Result<void> override;
  [[nodiscard]] auto Prepare() -> Result<void> override;
  [[nodiscard]] auto Poll() -> Result<void> override;
  [[nodiscard]] auto GetContext() const -> const AdapterCtx & override;
  [[nodiscard]] auto GetAdapterMetric() -> AdapterMetric override;
  auto PublishStateMetric(StateMetric metric) -> void override;
  void Start() override;

  // Cycle
  void EnterCycle();
  void CycleGuard(const std::stop_token &st);
  void SigHardFault();
  void Stop();

  // Setup
  [[nodiscard]] auto ConfigureAdapter() -> Result<void>;

  // Read
  [[nodiscard]] auto StartSnap7AreaRead(ReadBatch &batch) -> Result<void>;
  [[nodiscard]] auto StartSnap7MultiRead(ReadBatch &batch) -> Result<void>;

  // Final data points
  [[nodiscard]] auto CreateDataPoints() -> Result<void>;
  [[nodiscard]] static auto CreateAreaDp(ReadBatch &batch) -> Result<void>;
  [[nodiscard]] static auto CreateMultiDp(ReadBatch &batch) -> Result<void>;

  // Debug
  void DbgPrintConfigItem();
  void DbgPrintConfig();
  void DbgPrintCurrentDp();
  void DbgPrintCurrentMetric();

  // Config
  const size_t id_;
  const std::string name_;
  S7Connection connection_{};
  Json config_data_;
  ConfigItem config_item_;
  AdapterMetric metric_;

  // Snap7
  bool reconn_active_{false};
  size_t old_pdu_{0};
  size_t current_pdu_{0};
  std::unique_ptr<ISnap7Client> client_;

  // State
  std::unique_ptr<State> state_;

  // Thread
  std::mutex wait_mtx_;
  std::mutex metric_mtx_;
  inline static std::mutex print_mtx_;
  std::condition_variable_any cv_;
  std::jthread worker_thread_;
};
} // namespace tkk::s7
