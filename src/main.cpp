// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/handler.hpp"
#include "tkk/core/util.hpp"
#include <thread>

/**
 * @brief main entry
 *
 * @return int
 */
auto main() -> int {
  tkk::adapter::Handler a1{TKK_CONFIG_DIR};

  auto res = a1.Start();
  if (!res) {
    tkk::log::LogErrors(std::move(res).error().Dump(), "handler");
  }
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(30));
  }
}