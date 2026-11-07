#include "tkk/data_sink/sink_engine.hpp"
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <SQLiteCpp/Exception.h>
#include <SQLiteCpp/Transaction.h>
#include <deque>
#include <mutex>
#include <string_view>
#include <vector>

namespace tkk::sink {
namespace {
const std::string kOpenCreateSinkTable = R"(
                              CREATE TABLE IF NOT EXISTS data (
                                  id        INTEGER PRIMARY KEY AUTOINCREMENT,
                                  quality       INTEGER NOT NULL,
                                  data          BLOB    NOT NULL,
                                  data_type     TEXT    NOT NULL,
                                  timestamp_ms  INTEGER NOT NULL,
                                  name          TEXT  NOT NULL
                              )
                              )";

}

SinkEngine::~SinkEngine() {
  insert_stmt_.reset();
  select_stmt_.reset();
  db_.reset();
}

auto SinkEngine::OpenDatabase() -> Result<void> {

  try {
    db_.emplace(db_path_ / "sink.sqlite", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    db_->exec(kOpenCreateSinkTable);

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
  insert_stmt_.emplace(*db_, "INSERT INTO data (quality, data, data_type, timestamp_ms, name) "
                             "VALUES (?, ?, ?, ?, ?)");
  select_stmt_.emplace(*db_, "SELECT id, quality, data, data_type, timestamp_ms, name FROM data");
}

void SinkEngine::Push(std::vector<DataPoint> &&data_points) {
  std::unique_lock lock(queue_mtx_);
  queue_.push_back(std::move(data_points));
}

auto SinkEngine::RunOnce() -> Result<void> {
  {
    std::unique_lock lock(queue_mtx_);
    swap_queue_.swap(queue_);
    queue_.clear();
  }
  if (auto res = WriteTransaction(); !res) {
    // Handle the error, e.g., log it
    return {};
    // On error, reinsert the local queue back into the main queue
    queue_.insert(queue_.begin(), swap_queue_.begin(), swap_queue_.end());
  }
  swap_queue_.clear();
  return {};
}

auto SinkEngine::WriteTransaction() -> Result<void> {
  namespace chr = std::chrono;
  try {
    // Use transaction to fsync all writes atomically (memory friendly)
    SQLite::Transaction transaction(*db_);
    for (const auto &vec : swap_queue_) {
      for (const auto &data_point : vec) {
        if (auto res = WriteEntry(data_point); !res) {
          // Handle the error, e.g., log it
        }
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
    insert_stmt_.emplace(*db_, "INSERT INTO data (quality, data, data_type, timestamp_ms, name) "
                               "VALUES (?, ?, ?, ?, ?)");
    insert_stmt_->bind(1, static_cast<int>(data_point.quality));
    insert_stmt_->bind(2, data_point.data.data(), static_cast<int>(data_point.data.size()));
    insert_stmt_->bind(3, data_point.type);
    insert_stmt_->bind(
        4, chr::duration_cast<chr::milliseconds>(data_point.timestamp.time_since_epoch()).count());
    insert_stmt_->bind(5, data_point.name);

  } catch (SQLite::Exception &e) {
    return Error(ErrC::kDataBaseError, ErrA::kFatal,
                 "Unable to write entry to SQLite sink database")
        .WithStr("what", e.what())
        .With("code", e.getErrorCode())
        .Fail();
  }

  return {};
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
