// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <optional>

namespace tkk::sink {

class Sink final {
private:
  std::optional<SQLite::Statement> insert_stmt_;
  std::optional<SQLite::Statement> select_stmt_;
  std::optional<SQLite::Database> db_;

  [[nodiscard]] auto OpenDatabase() -> Result<void>;
  [[nodiscard]] auto WriteEntry(DataPoint &data_point) -> Result<void>;
  [[nodiscard]] auto ReadEntry() -> Result<void>;

public:
};

} // namespace tkk::sink
