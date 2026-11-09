// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/handler.hpp"
#include "tkk/core/util.hpp"
#include "tkk/data_sink/sink_worker.hpp"
#include <thread>

/**
 * @brief main entry
 *
 * @return int
 */
auto main() -> int {
  tkk::sink::SinkEngine e1{TKK_DB_DIR};
  tkk::sink::SinkWorker s1{e1};
  tkk::adapter::Handler a1{e1, TKK_CONFIG_DIR};

  auto res_handler = a1.Start();
  auto res_sink = s1.Start();
  if (!res_handler) { tkk::log::LogErrors(std::move(res_handler).error().Dump(), "handler"); }
  if (!res_sink) { tkk::log::LogErrors(std::move(res_sink).error().Dump(), "sink"); }
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(30));
  }
}