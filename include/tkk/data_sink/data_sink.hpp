// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace tkk::sink {

class Sink final {
public:
  explicit Sink(std::filesystem::path db_path = kDefDbPath) noexcept
      : db_path_(std::move(db_path)) {};

  [[nodiscard]] auto Start() -> Result<void>;

private:
  void Run(const std::stop_token &st);
  [[nodiscard]] auto GetStatus() -> int;
  [[nodiscard]] auto OpenDatabase() -> Result<void>;
  [[nodiscard]] auto WriteEntry(const DataPoint &data_point) -> Result<void>;
  [[nodiscard]] auto WriteTransaction(const std::vector<DataPoint> &batch) -> Result<void>;
  [[nodiscard]] auto ReadEntry() -> Result<void>;

  // setup
  static constexpr std::string_view kDefDbPath = "/etc/tkk-iotgateway/db";
  const std::filesystem::path db_path_;
  std::optional<SQLite::Statement> insert_stmt_;
  std::optional<SQLite::Statement> select_stmt_;
  std::optional<SQLite::Database> db_;

  // thread
  std::mutex wait_mtx_;
  std::mutex status_mtx_;
  std::condition_variable_any cv_;
  bool stop_requested_{false};
  std::jthread worker_thread_;

  // sink
  std::deque<DataPoint> queue_;
};

} // namespace tkk::sink
