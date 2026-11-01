#include "tkk/data_sink/data_sink.hpp"
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <SQLiteCpp/Exception.h>
#include <SQLiteCpp/Transaction.h>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <string_view>
#include <utility>

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

auto Sink::OpenDatabase() -> Result<void> {

  try {
    db_.emplace(db_path_ / "sink.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    db_->exec(kOpenCreateSinkTable);

    // Initialize prepared statements for insert and select operations
    insert_stmt_.emplace(*db_, "INSERT INTO data (quality, data, data_type, timestamp_ms, name) "
                               "VALUES (?, ?, ?, ?, ?)");
    select_stmt_.emplace(*db_, "SELECT id, quality, data, data_type, timestamp_ms, name FROM data");

  } catch (const SQLite::Exception &e) {
    return Error(ErrC::kDataBaseError, ErrA::kFatal,
                 "Unable to open or create SQLite sink database")
        .WithStr("path", db_path_.c_str())
        .WithStr("what", e.what())
        .With("code", e.getErrorCode())
        .Fail();
  }
  return {};
}

auto Sink::WriteEntry(const DataPoint &data_point) -> Result<void> {
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

auto Sink::WriteTransaction(const std::vector<DataPoint> &batch) -> Result<void> {
  namespace chr = std::chrono;
  try {
    // Use transaction to fsync all writes atomically (memory friendly)
    SQLite::Transaction transaction(*db_);
    for (const auto &data_point : batch) {
      if (auto res = WriteEntry(data_point); !res) {
        // Handle the error, e.g., log it
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

// auto Sink::ReadEntry() -> Result<void> {
//   return {};
// }

auto Sink::Start() -> Result<void> {

  if (auto res = OpenDatabase(); !res) { return res; }
  std::cout << "DBG: Database opened successfully" << "\n";

  worker_thread_ = std::jthread([this](const std::stop_token &st) { Run(st); });
  return {};
}

void Sink::Run(const std::stop_token &st) {
  std::unique_lock lock(wait_mtx_);

  while (true) {
    cv_.wait(lock, st, [&] -> bool { return !queue_.empty(); });
    if (stop_requested_ && queue_.empty()) { break; }
    auto batch = std::exchange(queue_, {});
    lock.unlock();
    for (auto &data_point : batch) {
      if (auto res = WriteEntry(data_point); !res) {
        // Handle the error, e.g., log it
      }
    }
    lock.lock();
  }
}

// auto Sink::GetStatus() -> int {
//   return 0;
// }
} // namespace tkk::sink