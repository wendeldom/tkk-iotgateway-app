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
#include <utility>
#include <vector>

namespace tkk::sink {

class SinkEngine : private NoCopyNoMove {
public:
  explicit SinkEngine(std::filesystem::path db_path) : db_path_(std::move(db_path)) {}

  ~SinkEngine();

  [[nodiscard]] auto GetStatus() -> int;
  [[nodiscard]] auto OpenDatabase() -> Result<void>;
  [[nodiscard]] auto RunOnce() -> Result<void>;
  [[nodiscard]] auto Flush() -> Result<void>;
  [[nodiscard]] auto DataAvailable() -> bool;
  [[nodiscard]] auto GetQueueSize() -> std::size_t;
  [[nodiscard]] auto GetTableSize() -> Result<std::size_t>;
  void Push(std::vector<DataPoint> &&data_points);

  void SetNotificationCv(std::condition_variable_any &noti_cv) { noti_cv_ = &noti_cv; }

private:
  [[nodiscard]] auto WriteEntry(const DataPoint &data_point) -> Result<void>;
  [[nodiscard]] auto WriteTransaction() -> Result<void>;
  [[nodiscard]] auto ReadEntry() -> Result<void>;
  void InitStatements();

  std::deque<std::vector<DataPoint>> queue_;
  std::deque<std::vector<DataPoint>> swap_queue_;

  std::mutex queue_mtx_;
  std::filesystem::path db_path_;
  std::optional<SQLite::Database> db_;
  std::optional<SQLite::Statement> insert_stmt_;
  std::optional<SQLite::Statement> select_stmt_;
  std::condition_variable_any *noti_cv_ = nullptr;
};

} // namespace tkk::sink
