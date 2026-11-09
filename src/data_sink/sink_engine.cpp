// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/data_sink/sink_engine.hpp"
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <SQLiteCpp/Exception.h>
#include <SQLiteCpp/Transaction.h>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string_view>
#include <vector>

namespace tkk::sink {
namespace {
const std::string kDataTableName = "data";
const std::string kDbName = "sink";
const std::string kColumnId = "id";
const std::string kColumnQuality = "quality";
const std::string kColumnData = "data";
const std::string kColumnDataType = "data_type";
const std::string kColumnTimestampMs = "timestamp_ms";
const std::string kColumnName = "name";
const std::string kCountTable = "SELECT COUNT(*) FROM " + kDataTableName;
const std::string kOpenCreateDataTable =
    "CREATE TABLE IF NOT EXISTS " + kDataTableName + " (" + kColumnId +
    " INTEGER PRIMARY KEY AUTOINCREMENT, " + kColumnQuality + " INTEGER NOT NULL, " + kColumnData +
    " BLOB NOT NULL, " + kColumnDataType + " TEXT NOT NULL, " + kColumnTimestampMs +
    " INTEGER NOT NULL, " + kColumnName + " TEXT NOT NULL)";

} // namespace

SinkEngine::~SinkEngine() {
  insert_stmt_.reset();
  select_stmt_.reset();
  db_.reset();
}

auto SinkEngine::OpenDatabase() -> Result<void> {

  try {
    db_.emplace(db_path_ / (kDbName + ".sqlite"), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    db_->exec(kOpenCreateDataTable);

    // Initialize prepared statements for insert and select operations
    InitStatements();

  } catch (const SQLite::Exception &e) {
    return Error(ErrC::kFileError, ErrA::kFatal, "Unable to open or create SQLite sink database")
        .WithStr("path", db_path_.c_str())
        .WithStr("what", e.what())
        .With("code", e.getErrorCode())
        .Fail();
  }
  return {};
}

void SinkEngine::InitStatements() {
  insert_stmt_.emplace(*db_, "INSERT INTO " + kDataTableName + " (" + kColumnQuality + ", " +
                                 kColumnData + ", " + kColumnDataType + ", " + kColumnTimestampMs +
                                 ", " + kColumnName +
                                 ") "
                                 "VALUES (?, ?, ?, ?, ?)");
  select_stmt_.emplace(*db_, "SELECT " + kColumnId + ", " + kColumnQuality + ", " + kColumnData +
                                 ", " + kColumnDataType + ", " + kColumnTimestampMs + ", " +
                                 kColumnName + " FROM " + kDataTableName);
}

void SinkEngine::Push(std::vector<DataPoint> &&data_points) {
  std::unique_lock lock(queue_mtx_);
  queue_.push_back(std::move(data_points));
  if (noti_cv_ != nullptr) { noti_cv_->notify_one(); }
}

auto SinkEngine::RunOnce() -> Result<void> {
  {
    std::unique_lock lock(queue_mtx_);
    swap_queue_.swap(queue_);
    queue_.clear();
  }
  if (auto res = WriteTransaction(); !res) {
    // On error, reinsert the local queue back into the main queue
    queue_.insert(queue_.begin(), swap_queue_.begin(), swap_queue_.end());
    return res;
  }
  swap_queue_.clear();
  return {};
}

auto SinkEngine::WriteTransaction() -> Result<void> {
  try {
    // Use transaction to fsync all writes atomically (memory friendly)
    SQLite::Transaction transaction(*db_);
    for (const auto &vec : swap_queue_) {
      for (const auto &data_point : vec) {
        if (auto res = WriteEntry(data_point); !res) { return res; }
      }
    }
    transaction.commit();
  } catch (const SQLite::Exception &e) {
    return Error(ErrC::kDataBaseError, ErrA::kFatal,
                 "Unable to write transaction to SQLite sink database")
        .WithStr("what", e.what())
        .With("code", e.getErrorCode())
        .Fail();
  }
  return {};
}

auto SinkEngine::WriteEntry(const DataPoint &data_point) -> Result<void> {
  namespace chr = std::chrono;
  try {
    insert_stmt_.reset();
    insert_stmt_.emplace(*db_, "INSERT INTO " + kDataTableName + " (" + kColumnQuality + ", " +
                                   kColumnData + ", " + kColumnDataType + ", " +
                                   kColumnTimestampMs + ", " + kColumnName +
                                   ") "
                                   "VALUES (?, ?, ?, ?, ?)");
    insert_stmt_->bind(1, static_cast<int>(data_point.quality));
    insert_stmt_->bind(2, data_point.data.data(), static_cast<int>(data_point.data.size()));
    insert_stmt_->bind(3, data_point.type);
    insert_stmt_->bind(
        4, chr::duration_cast<chr::milliseconds>(data_point.timestamp.time_since_epoch()).count());
    insert_stmt_->bind(5, data_point.name);
    insert_stmt_->exec();

  } catch (SQLite::Exception &e) {
    return Error(ErrC::kDataBaseError, ErrA::kFatal,
                 "Unable to write entry to SQLite sink database")
        .WithStr("what", e.what())
        .With("code", e.getErrorCode())
        .Fail();
  }

  return {};
}

auto SinkEngine::DataAvailable() -> bool {
  std::unique_lock lock(queue_mtx_);
  return !queue_.empty();
}

auto SinkEngine::GetQueueSize() -> std::size_t {
  std::unique_lock lock(queue_mtx_);
  return queue_.size();
}

auto SinkEngine::GetTableSize() -> Result<std::size_t> {
  if (!db_) { return Error(ErrC::kDataBaseError, ErrA::kFatal, "Database not open").Fail(); }
  try {
    int64_t count = db_->execAndGet(kCountTable).getInt64();
    return count;
  } catch (const SQLite::Exception &e) {
    return Error(ErrC::kDataBaseError, ErrA::kFatal,
                 "Unable to get table size from SQLite sink database")
        .WithStr("what", e.what())
        .With("code", e.getErrorCode())
        .Fail();
  }
}

// Reading (not implemented yet)

// auto SinkEngine::ReadEntry() -> Result<void> {
//   return {};
// }

// Status (not implemented yet)

// auto SinkEngine::GetStatus() -> int {
//   return 0;
// }
} // namespace tkk::sink
