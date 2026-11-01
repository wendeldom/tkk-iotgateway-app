// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/adapter/data_source_adapter.hpp"
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <unordered_set>
#include <vector>

namespace tkk::adapter {

struct AdapterItem {
  AdapterInfo info;
  std::filesystem::path path;
  std::unique_ptr<IDataSourceAdapter> adapter;
  std::chrono::time_point<std::chrono::steady_clock> restart_at;
  size_t tot_restarts{0};
  bool hard_fault{false};
  bool restart_set{false};
};

struct HandlerStatus {
  size_t adapter_count{0};
  size_t tot_restarts{0};
  std::vector<size_t> active_ids;
};

class Handler final : private NoCopyNoMove {
public:
  explicit Handler(std::filesystem::path path = kDefConfigPath) noexcept
      : config_path_(std::move(path)), s7_path_(config_path_ / "s7"),
        opc_path_(config_path_ / "opcua") {}

  ~Handler();

  [[nodiscard]] auto Start() -> Result<void>;
  [[nodiscard]] auto GetStatus() -> HandlerStatus;

private:
  [[nodiscard]] auto SearchConfigDirs() -> Result<void>;
  [[nodiscard]] auto SearchS7Path() -> Result<void>;
  [[nodiscard]] auto SearchOpcPath() -> Result<void>;
  [[nodiscard]] auto CreateS7Adapter(const std::filesystem::path &path) -> Result<AdapterItem>;
  [[nodiscard]] auto CreateOpcAdapter(const std::filesystem::path &path) -> Result<AdapterItem>;
  [[nodiscard]] auto IsDuplicateId(size_t id) -> bool;
  void SetupAdapterRestart(AdapterItem &adp_item);
  [[nodiscard]] auto RestartAdapter(AdapterItem &adp_item) -> Result<void>;
  void PollAdapterMetrics(const std::stop_token &st);
  void PublishStatus();
  void DeleteId(size_t id);

  // setup
  static constexpr std::string_view kDefConfigPath = "/etc/tkk-iotgateway";
  const std::filesystem::path config_path_;
  const std::filesystem::path s7_path_;
  const std::filesystem::path opc_path_;
  size_t tot_restarts_{0};
  HandlerStatus status_{};
  std::vector<AdapterItem> adapters_;
  std::unordered_set<size_t> active_ids_;

  // thread
  static constexpr size_t kDefaultPollDelay = 5000;
  static constexpr size_t kDefaultRestartDelay = 30000;
  std::mutex wait_mtx_;
  std::mutex status_mtx_;
  std::condition_variable_any cv_;
  bool stop_requested_{false};
  std::jthread worker_thread_;
};
} // namespace tkk::adapter