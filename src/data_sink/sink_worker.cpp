// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/data_sink/sink_worker.hpp"
#include "tkk/data_sink/sink_engine.hpp"
#include <iostream>
#include <stop_token>

namespace tkk::sink {

SinkWorker::~SinkWorker() {
  Stop();
}

auto SinkWorker::Start() -> Result<void> {

  if (auto res = engine_.OpenDatabase(); !res) { return res; }
  std::cout << "DBG: Database opened successfully" << "\n";

  worker_thread_ = std::jthread([this](const std::stop_token &st) -> void { Run(st); });
  return {};
}

void SinkWorker::Stop() {

  if (worker_thread_.joinable()) {
    worker_thread_.request_stop();
    worker_thread_.join();
  }
}

void SinkWorker::Run(const std::stop_token &st) {
  std::unique_lock lock(wait_mtx_);

  while (!st.stop_requested()) {
    cv_.wait(lock, st, [&] -> bool { return engine_.DataAvailable(); });
    if (auto res = engine_.RunOnce(); !res) {
      // Handle the error, e.g., log it
    }
    if (st.stop_requested()) {
      if (auto res = engine_.RunOnce(); !res) {
        // Handle the error, e.g., log it
      }
      break;
    }
    // auto batch = std::exchange(queue_, {});
    // lock.unlock();
    // for (auto &data_point : batch) {
    //   if (auto res = WriteEntry(data_point); !res) {
    //     // Handle the error, e.g., log it
    //   }
    // }
    // lock.lock();
  }
}

} // namespace tkk::sink
