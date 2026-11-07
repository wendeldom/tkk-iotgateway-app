// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/core/error.hpp"
#include "tkk/data_sink/sink_engine.hpp"
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>

namespace tkk::sink {

class SinkWorker {
public:
  explicit SinkWorker(SinkEngine &engine) : engine_(engine) {}

  ~SinkWorker();

  [[nodiscard]] auto Start() -> Result<void>;
  void Stop();

private:
  void Run(const std::stop_token &st);

  // setup

  // sink
  SinkEngine &engine_;

  // thread
  std::mutex wait_mtx_;
  std::mutex status_mtx_;
  std::condition_variable_any cv_;
  std::jthread worker_thread_;
};

} // namespace tkk::sink
