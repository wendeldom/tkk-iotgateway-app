#include "tkk/data_sink/data_sink.hpp"
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <SQLiteCpp/Exception.h>
#include <string_view>

namespace tkk::sink {
namespace {
const std::string kOpenCreateSinkTable = R"(
                              CREATE TABLE IF NOT EXISTS data (
                                  id        INTEGER PRIMARY KEY AUTOINCREMENT,
                                  quality       TEXT    NOT NULL,
                                  data          BLOB    NOT NULL,
                                  data_type     TEXT    NOT NULL,
                                  timestamp_ms  INTEGER NOT NULL,
                                  name          TEXT  NOT NULL
                              )
                              )";

}

auto Sink::OpenDatabase() -> Result<void> {

  try {
    db_.emplace("sink.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    db_->exec(kOpenCreateSinkTable);

    // Initialize prepared statements for insert and select operations
    insert_stmt_.emplace(*db_, "INSERT INTO data (quality, data, data_type, timestamp_ms, name) "
                               "VALUES (?, ?, ?, ?, ?)");
    select_stmt_.emplace(*db_, "SELECT id, quality, data, data_type, timestamp_ms, name FROM data");

  } catch (const SQLite::Exception &e) {
    return Error(ErrC::kUnknown, ErrA::kFatal, "Unable to open or create SQLite sink database")
        .WithStr("what", e.what())
        .With("code", e.getErrorCode())
        .Fail();
  }
  return {};
}

auto Sink::WriteEntry(DataPoint &data_point) -> Result<void> {
  try {
    insert_stmt_.reset();
    insert_stmt_.emplace(*db_, "INSERT INTO data (quality, data, data_type, timestamp_ms, name) "
                               "VALUES (?, ?, ?, ?, ?)");
    insert_stmt_->bind(1, static_cast<int>(data_point.quality));

  } catch (SQLite::Exception &e) {
    return Error(ErrC::kUnknown, ErrA::kFatal, "Unable to write entry to SQLite sink database")
        .WithStr("what", e.what())
        .With("code", e.getErrorCode())
        .Fail();
  }

  return {};
}

auto Sink::ReadEntry() -> Result<void> {
  return {};
}
} // namespace tkk::sink