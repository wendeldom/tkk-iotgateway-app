// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/handler.hpp"
#include "tkk/adapter/s7/adapter.hpp"
#include "tkk/adapter/s7/config_validation.hpp"
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include "tkk/core/util.hpp"
#include <chrono>
#include <cstddef>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string_view>
#include <utility>

namespace tkk::adapter {

namespace {

constexpr std::string_view kErrSource = "Handler";

[[nodiscard]] auto OpenAndParse(const std::filesystem::path &path) -> Result<Json> {

  std::ifstream file(path);
  if (!file.is_open()) {
    return Error(ErrC::kFileError, ErrA::kFatal, "unable to open file")
        .WithStr("path", path.c_str())
        .Fail();
  }

  auto config = Json::parse(file, nullptr, false);
  if (config.is_discarded()) {
    return Error(ErrC::kParseError, ErrA::kFatal, "file format error")
        .WithStr("path", path.c_str())
        .Fail();
  }

  return config;
}

} // namespace

Handler::~Handler() {
  if (worker_thread_.joinable()) {
    worker_thread_.request_stop();
    worker_thread_.join();
  }
}

auto Handler::Start() -> Result<void> {
  auto res = SearchConfigDirs();
  PublishStatus();
  worker_thread_ = std::jthread([this](const std::stop_token &st) { PollAdapterMetrics(st); });
  if (!res) {
    return res;
  }
  return {};
}

auto Handler::SearchConfigDirs() -> Result<void> {
  namespace fs = std::filesystem;

  Error err{ErrC::kDefaultParent, ErrA::kSkip, ""};

  if (!fs::is_directory(config_path_)) {
    return Error(ErrC::kFileError, ErrA::kFatal, "config directory not found")
        .WithStr("path", config_path_.c_str())
        .Fail();
  }

  // s7 adapter path
  if (auto res = SearchS7Path(); !res) {
    err.Add(std::move(res).error());
  }
  // opc adapter path
  if (auto res = SearchOpcPath(); !res) {
    err.Add(std::move(res).error());
  }

  if (err.HasSubErr()) {
    return std::move(err).Fail();
  }
  return {};
}

auto Handler::SearchS7Path() -> Result<void> {
  namespace fs = std::filesystem;

  Error err{ErrC::kDefaultParent, ErrA::kSkip, ""};

  if (!fs::is_directory(s7_path_)) {
    // This is more of an "info", so we dont return errors if there is only a
    // not used subdir missing
    auto err = Error(ErrC::kFileError, ErrA::kSkip, "subdirectory not found")
                   .WithStr("path", s7_path_.c_str());
    log::LogErrors(std::move(err).Dump(), "handler");
    return {};
  }

  // Iterate over all files
  for (const auto &file : fs::directory_iterator(s7_path_)) {
    if (file.is_regular_file() && file.path().extension() == ".json") {
      auto res = CreateS7Adapter(file.path());
      if (!res) {
        err.Add(std::move(res).error());
        continue;
      }
      adapters_.push_back(std::move(*res));
    }
  }

  if (err.HasSubErr()) {
    return std::move(err).Fail();
  }
  return {};
}

auto Handler::SearchOpcPath() -> Result<void> {
  namespace fs = std::filesystem;

  Error err{ErrC::kDefaultParent, ErrA::kSkip, ""};

  if (!fs::is_directory(opc_path_)) {
    // This is more of an "info", so we dont return errors if there is only a
    // not used subdir missing
    auto err = Error(ErrC::kFileError, ErrA::kSkip, "subdirectory not found")
                   .WithStr("path", opc_path_.c_str());
    log::LogErrors(std::move(err).Dump(), "handler");
    return {};
  }

  // Iterate over all files
  for (const auto &file : fs::directory_iterator(opc_path_)) {
    if (file.is_regular_file() && file.path().extension() == ".json") {
      // auto res = CreateOpcAdapter(file.path());
      // if (!res) {
      //   err.Add(std::move(res).error());
      //   continue;
      // }
      // adapters_.push_back(std::move(*res));
    }
  }

  if (err.HasSubErr()) {
    return std::move(err).Fail();
  }
  return {};
}

auto Handler::CreateS7Adapter(const std::filesystem::path &path) -> Result<AdapterItem> {
  // Open file
  auto res_open = OpenAndParse(path);
  if (!res_open) {
    return std::move(res_open).error().Fail();
  }
  // Extract protocol,id,name
  auto res_params = s7::GetAdapterParams(*res_open);
  if (!res_params) {
    return std::move(res_params).error().Fail();
  }
  // Check for duplicate adapter ids
  if (IsDuplicateId(res_params->id)) {
    return Error(ErrC::kConfigInvalid, ErrA::kFatal, "duplicate adapter id")
        .WithStr("file", path.c_str())
        .Fail();
  }
  // Fill adapter info
  AdapterItem adp_item;
  adp_item.info = *res_params;
  adp_item.path = path;
  adp_item.adapter =
      std::make_unique<s7::Adapter>(std::move(*res_open), res_params->id, res_params->name);

  return adp_item;
}

void Handler::SetupAdapterRestart(AdapterItem &adp_item) {
  namespace chr = std::chrono;
  adp_item.hard_fault = true;
  adp_item.restart_at = chr::steady_clock::now() + chr::milliseconds(kDefaultRestartDelay);
  adp_item.restart_set = true;
  adp_item.adapter.reset(); // Delete the current adapter
  DeleteId(adp_item.info.id);
}

auto Handler::RestartAdapter(AdapterItem &adp_item) -> Result<void> {
  namespace fs = std::filesystem;
  Result<AdapterItem> res;

  switch (adp_item.info.protocol) {
  case ProtocolType::kS7:
    res = CreateS7Adapter(adp_item.path);
    break;
  case ProtocolType::kOpcUa:
    //  res = CreateOpcAdapter(adp_item.path);
    break;
  case ProtocolType::kUnset:
    assert(false);
    std::abort();
  }

  if (!res) {
    SetupAdapterRestart(adp_item);
    return std::move(res).error().WithView("when", "during adapter restart").Fail();
  }
  res->tot_restarts = adp_item.tot_restarts + 1; // Take the existing counter and inc. it
  tot_restarts_++;
  adp_item = std::move(*res); // Move the new adapter into the existing slot
  return {};
}

auto Handler::IsDuplicateId(size_t id) -> bool {
  auto [it, inserted] = active_ids_.insert(id);
  return !inserted;
}

void Handler::DeleteId(size_t id) {
  active_ids_.erase(id);
}

void Handler::PollAdapterMetrics(const std::stop_token &st) {
  namespace chr = std::chrono;

  std::unique_lock lock(wait_mtx_);
  // Main thread loop
  while (!st.stop_requested()) {
    // Poll every adapter metric
    for (auto &adapter_item : adapters_) {

      // Check if adapter is setup for restart
      if (adapter_item.restart_set && (chr::steady_clock::now() >= adapter_item.restart_at)) {
        auto res = RestartAdapter(adapter_item);
        if (!res) {
          log::LogErrors(std::move(res).error().Dump(), kErrSource);
        }
        continue;
      }

      // From here on only allowed if the adapter ptr is valid
      if (adapter_item.adapter == nullptr) {
        continue;
      }
      const auto metric = adapter_item.adapter->GetAdapterMetric();

      // Check if adapter is in hardfault -> setup for restart
      if (metric.hard_fault && !adapter_item.restart_set) {
        SetupAdapterRestart(adapter_item);
        continue;
      }
      // Check if adapter is started
      if (!metric.started) {
        adapter_item.adapter->Start();
      }
    }

    PublishStatus();
    // Poll delay
    cv_.wait_for(lock, st, std::chrono::milliseconds(kDefaultPollDelay), [] { return false; });
  }
}

void Handler::PublishStatus() {
  HandlerStatus status{};
  status.adapter_count = adapters_.size();
  status.tot_restarts = tot_restarts_;
  for (const auto &id : active_ids_) {
    status.active_ids.push_back(id);
  }

  std::scoped_lock lock(status_mtx_);
  status_ = std::move(status);
}

auto Handler::GetStatus() -> HandlerStatus {
  std::scoped_lock lock(status_mtx_);
  return status_;
}

} // namespace tkk::adapter
