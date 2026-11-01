// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/adapter.hpp"
#include "tkk/adapter/s7/batch_creation.hpp"
#include "tkk/adapter/s7/client.hpp"
#include "tkk/adapter/s7/config_validation.hpp"
#include "tkk/adapter/s7/error.hpp"
#include "tkk/adapter/s7/helper.hpp"
#include "tkk/adapter/s7/state.hpp"
#include "tkk/core/type.hpp"
#include <format>
#include <iostream>
#include <memory>

// #include <print>

namespace tkk::s7 {

Adapter::Adapter(Json &&config, size_t id, std::string name, std::unique_ptr<ISnap7Client> client,
                 std::unique_ptr<State> state)
    : id_{id}, name_{std::move(name)}, config_data_(std::move(config)) {

  // !=nullptr is for tests (fake client/state)
  client != nullptr ? client_ = std::move(client) : client_ = std::make_unique<Snap7Client>();
  state != nullptr ? state_ = std::move(state) : state_ = std::make_unique<InitState>();

  // temporary assignments for more precise logging
  config_item_.ctx.id = id_;
  config_item_.ctx.name = name_;
}

Adapter::~Adapter() {
  Stop();
}

/**
 * @brief Start the adapter
 *
 */
void Adapter::Start() {
  metric_.started = true;
  EnterCycle();
}

/**
 * @brief Stop the adapter
 *
 */
void Adapter::Stop() {
  if (worker_thread_.joinable()) {
    worker_thread_.request_stop();
    worker_thread_.join();
  }
}

/**
 * @brief Etner adapter loop
 *
 */
void Adapter::EnterCycle() {
  // No exceptions should appear, but we never know for sure
  worker_thread_ = std::jthread([this](const std::stop_token &st) {
    try {
      CycleGuard(st);
    } catch (const std::exception &e) {
      // std::println("EXCEPTION thrown: {}", e.what());
      SigHardFault();
    } catch (...) {
      // std::println("EXCEPTION thrown (unkown)");
      SigHardFault();
    }
    // Wait to get picked up by the handler :)
    std::unique_lock lock(wait_mtx_);
    // NOLINTNEXTLINE (cppcoreguidelines-avoid-magic-numbers)
    cv_.wait_for(lock, st, std::chrono::seconds(30), [] { return false; });
  });
}

/**
 * @brief Main adapter loop
 *
 */
void Adapter::CycleGuard(const std::stop_token &st) {
  std::unique_lock lock(wait_mtx_);

  // Main thread loop
  while (!st.stop_requested()) {

    auto next = state_->Update(*this);

    cv_.wait_for(lock, st, std::chrono::milliseconds(state_->GetDelay()), [] { return false; });

    if (next != nullptr) {
      state_ = std::move(next);
    }
  }
}

/**
 * @brief IF initilaize the adapter
 *
 * @return Result<void>
 */
auto Adapter::Init() -> Result<void> {
  if (auto res = ConfigureAdapter(); !res) {
    return res;
  }
  return {};
}

/**
 * @brief IF connect to PLC
 *
 * @return Result<void>
 */
auto Adapter::Connect() -> Result<void> {
  using error::GetSnapErrInfo;

  auto result = client_->ConnectTo(connection_.ip.c_str(), connection_.rack, connection_.slot);
  if (result != 0) {
    return Error(ErrC::kConnectionFailed, ErrA::kReconnect, "")
        .With("code", result)
        .WithView("info", GetSnapErrInfo(result).text)
        .Fail();
  }
  current_pdu_ = client_->GetPduLength();
  return {};
}

/**
 * @brief IF reconnect sequence
 *
 * @return Result<void>
 */
auto Adapter::Reconnect() -> Result<void> {
  reconn_active_ = true;
  old_pdu_ = current_pdu_;

  if (client_->Connected()) {
    client_->Reset(false);
    client_->Disconnect();
  }
  return Connect();
}

/**
 * @brief IF prepare the adapter
 *
 * @return Result<void>
 */
auto Adapter::Prepare() -> Result<void> {

  // Skip prepare step, if pdu is same as before (Config is heavily pdu
  // dependent)
  if (reconn_active_ && (old_pdu_ == current_pdu_)) {
    reconn_active_ = false;
    return {};
  }

  auto res = Creator{config_data_, client_->GetPduLength()}.Create();
  if (!res) {
    return std::move(res).error().Fail();
  }
  config_item_ = std::move(*res);
  // Set id + name again (1. time in constructor) bcs. we got a new config
  // item
  config_item_.ctx.id = id_;
  config_item_.ctx.name = name_;

  reconn_active_ = false;
  return {};
}

/**
 * @brief IF poll snap7 reads
 *
 * @return Result<void>
 */
auto Adapter::Poll() -> Result<void> {
  using enum ReadMode;

  std::chrono::time_point timestamp = std::chrono::system_clock::now();
  Error err(ErrC::kDefaultParent, ErrA::kSkip, "");

  for (auto &batch : config_item_.batches) {
    batch.header.timestamp = timestamp;

    switch (batch.header.mode) {
    case kArea:
      if (auto res = StartSnap7AreaRead(batch); !res) {
        err.Add(std::move(res).error());
      }
      break;
    case kMulti:
      if (auto res = StartSnap7MultiRead(batch); !res) {
        err.Add(std::move(res).error());
      }
      break;
    case kUnset:
      assert(false);
      std::abort();
    }
    // Check if an error needs a reconnect, if so, return, otherwise just keep
    // reading
    if (err.GetAction() >= ErrA::kReconnect) {
      return std::move(err).Fail();
    }
  }

  if (auto res = CreateDataPoints(); !res) {
    err.Add(std::move(res).error());
  }

  if (err.HasSubErr()) {
    return std::move(err).Fail();
  }
  return {};
}

/**
 * @brief Start a snap7 area read
 *
 * @param batch_
 * @return Result<void>
 */
auto Adapter::StartSnap7AreaRead(ReadBatch &batch) -> Result<void> {
  using error::GetSnapErrInfo;

  const ReadHeader &head = batch.header;

  auto cast = [](const U32 &val) -> int { return static_cast<int>(val); }; // Saves some typing

  auto result = client_->ReadArea(head.target, cast(head.number), cast(head.offset),
                                  cast(head.read_buffer_size), S7WLByte, batch.buffer.data());

  batch.header.status = result;

  if (result != 0) {
    return Error(ErrC::kReadError, GetSnapErrInfo(result).action, "bad area read result")
        .With("code", result)
        .WithView("info", GetSnapErrInfo(result).text)
        .Fail();
  }
  return {};
}

/**
 * @brief Start an snap7 multivar read
 *
 * @param batch_
 * @return Result<void>
 */
auto Adapter::StartSnap7MultiRead(ReadBatch &batch) -> Result<void> {
  using error::GetSnapErrInfo;

  Error err(ErrC::kDefaultParent, ErrA::kSkip, "");

  auto cast = [](const U32 &val) -> int { return static_cast<int>(val); }; // Saves some typing

  auto result =
      client_->ReadMultiVars(batch.snap7_data_items.data(), cast(batch.snap7_data_items.size()));

  batch.header.status = result;

  if (result != 0) {
    err.Add(Error(ErrC::kReadError, GetSnapErrInfo(result).action, "bad multi read result")
                .With("code", result)
                .WithView("info", GetSnapErrInfo(result).text));
  }

  for (const auto &item : batch.snap7_data_items) {
    if (item.Result != 0) {
      err.Add(Error(ErrC::kReadError, GetSnapErrInfo(item.Result).action, "bad item result")
                  .With("code", item.Result)
                  .WithView("info", GetSnapErrInfo(item.Result).text));
    }
  }
  if (err.HasSubErr()) {
    return std::move(err).Fail();
  }
  return {};
}

/**
 * @brief Create generic data points
 *
 */
auto Adapter::CreateDataPoints() -> Result<void> {
  using enum ReadMode;

  Error err{ErrC::kDefaultParent, ErrA::kSkip, ""};

  for (auto &batch : config_item_.batches) {
    switch (batch.header.mode) {
    case kArea:
      if (auto res = CreateAreaDp(batch); !res) {
        err.Add(std::move(res).error());
      }
      break;
    case kMulti:
      if (auto res = CreateMultiDp(batch); !res) {
        err.Add(std::move(res).error());
      }
      break;
    case kUnset:
      assert(false);
      std::abort();
    }
  }

  if (err.HasSubErr()) {
    return std::move(err).Fail();
  }
  return {};
}

auto Adapter::CreateAreaDp(ReadBatch &batch) -> Result<void> {

  Error err(ErrC::kDefaultParent, ErrA::kSkip, "");

  Quality quality{Quality::kBad};
  auto buffer_view = SpanU8(batch.buffer);

  batch.data_points.clear();

  // Only have th whole read result as quality indicator in area read
  if (batch.header.status == 0) {
    quality = Quality::kGood;
  }

  for (const auto &tag : batch.tags) {
    DataPoint dp{};
    dp.id = tag.id;
    dp.quality = quality;
    dp.name = tag.name;
    dp.timestamp = batch.header.timestamp;
    dp.type = detail::DataTypeToString(tag.type);

    if (dp.quality == Quality::kGood) {

      if (auto res_ext =
              detail::ExtractBytes(buffer_view, tag.offset, detail::GetItemReadSize(tag));
          !res_ext) {
        dp.quality = Quality::kBad;
        err.Add(std::move(res_ext).error());
      } else if (auto res_cnv = detail::SwapBytes(*res_ext, tag); !res_cnv) {
        dp.quality = Quality::kBad;
        err.Add(std::move(res_cnv).error());
      } else {
        dp.data = std::move(*res_cnv);
      }
    }

    batch.data_points.push_back(std::move(dp));
  }

  if (err.HasSubErr()) {
    return std::move(err).Fail();
  }
  return {};
}

auto Adapter::CreateMultiDp(ReadBatch &batch) -> Result<void> {
  using enum Quality;

  Error err(ErrC::kDefaultParent, ErrA::kSkip, "");

  U32 offset{0};
  size_t idx{0};
  auto buffer_view = SpanU8(batch.buffer);

  batch.data_points.clear();

  for (const auto &tag : batch.tags) {
    DataPoint dp{};
    dp.id = tag.id;
    dp.name = tag.name;
    dp.timestamp = batch.header.timestamp;
    dp.type = detail::DataTypeToString(tag.type);

    batch.snap7_data_items[idx].Result == 0 ? dp.quality = Quality::kGood
                                            : dp.quality = kBad; // Single item result
    batch.header.status != 0 ? dp.quality = kBad : dp.quality;   // Whole multi read result

    if (dp.quality == Quality::kGood) {

      if (auto res_ext = detail::ExtractBytes(buffer_view, offset, detail::GetItemReadSize(tag));
          !res_ext) {
        dp.quality = kBad;
        err.Add(std::move(res_ext).error());
      } else if (auto res_cnv = detail::SwapBytes(*res_ext, tag); !res_cnv) {
        dp.quality = kBad;
        err.Add(std::move(res_cnv).error());
      } else {
        dp.data = std::move(*res_cnv);
      }
    }

    batch.data_points.push_back(std::move(dp));
    offset += detail::GetItemReadSize(tag);
    idx++;
  }

  if (err.HasSubErr()) {
    return std::move(err).Fail();
  }
  return {};
}

/**
 * @brief Configure adapter
 *
 * @return Result<void>
 */
auto Adapter::ConfigureAdapter() -> Result<void> {

  auto res = Validator(std::move(config_data_)).Validate();

  if (!res) {
    return std::move(res).error().Fail();
  }
  config_data_ = std::move(*res);
  connection_ = GetConnParams(config_data_);
  return {};
}

auto Adapter::PublishStateMetric(StateMetric metric) -> void {
  std::scoped_lock lock{metric_mtx_};
  metric_ += metric;
}

auto Adapter::GetAdapterMetric() -> AdapterMetric {
  std::scoped_lock lock{metric_mtx_};
  return metric_;
}

auto Adapter::GetContext() const -> const AdapterCtx & {
  return config_item_.ctx;
}

void Adapter::SigHardFault() {
  std::scoped_lock lock{metric_mtx_};
  metric_.hard_fault = true;
}

/**
void Adapter::DbgPrintConfigItem() {
  std::scoped_lock lock{print_mtx_};

  std::println();
  std::println("<---Batch bucket info--->");
  std::println("tot read size:        {} bytes", config_item_.total_read_size);
  std::println("tot batch amount:     {} batches", config_item_.total_batch);
  std::println("tot DP amount:        {} data points", config_item_.total_dp);
  std::println();
  for (const auto &batch : config_item_.batches) {
    std::println();
    std::println("<--New batch-->");
    std::println("no:                   {}", batch.header.batch_no);
    std::println("mode:                 {}",
                 (batch.header.mode == ReadMode::kArea) ? "area" : "multi");
    std::println("item count:           {}", batch.header.item_count);
    std::println("offset:               {} bytes", batch.header.offset);
    std::println("target:               {}", batch.header.target);
    std::println("read buffer size:     {} bytes", batch.header.read_buffer_size);
    std::println("req pdu size:         {} bytes", batch.header.request_pdu_size);
    std::println("res pdu size:         {} bytes", batch.header.response_pdu_size);
  }
  std::println();
  std::println();
}

void Adapter::DbgPrintConfig() {
  std::scoped_lock lock{print_mtx_};
  for (const auto &batch : config_item_.batches) {

    std::println();
    std::println();
    std::println("<---New batch--->");
    std::println("mode:      {}", (batch.header.mode == ReadMode::kArea) ? "area" : "multi");
    std::println("target:    {}", static_cast<int>(batch.header.target));
    std::println("offset:    {}", batch.header.offset);
    std::println("req pdu:   {}", batch.header.request_pdu_size);
    std::println("res pdu:   {}", batch.header.response_pdu_size);
    std::println("items:     {}", batch.header.item_count);
    std::println();
    std::println("<--Tags-->");
    std::println();
    for (const auto &tag : batch.tags) {
      std::println();
      std::println("<-Tag->");
      std::println("id:        {}", tag.id);
      std::println("name:      {}", tag.name);
      std::println("target:    {}", static_cast<int>(tag.target));
      std::println("number:    {}", tag.number);
      std::println("offset:    {}", tag.offset);
      std::println("length:    {}", tag.length);
      std::println("type:      {}", detail::DataTypeToString(tag.type));
      std::println("bit:       {}", static_cast<int>(tag.bit));
    }
  }
}
*/
void Adapter::DbgPrintCurrentDp() {

  std::scoped_lock lock{print_mtx_};
  for (const auto &batch : config_item_.batches) {
    for (const auto &dp : batch.data_points) {
      std::cout << "\n";
      std::cout << "<-DataPoint->" << "\n";
      std::cout << std::format("id:        {}", dp.id) << "\n";
      std::cout << std::format("quality:   {}", static_cast<int>(dp.quality)) << "\n";
      std::cout << std::format("name:      {}", dp.name) << "\n";
      std::cout << std::format("time:      {}", dp.timestamp) << "\n";
      std::cout << std::format("type:      {}", dp.type) << "\n";
      std::cout << "data:      ";
      for (const auto &byte : dp.data) {
        std::cout << std::format("{:02X} ", byte);
      }
      std::cout << "\n";
    }
  }
}

/*
void Adapter::DbgPrintCurrentMetric() {
  std::scoped_lock lock{print_mtx_};
  std::println("[{}]---Adapter Metric updated---", config_item_.ctx.id);
  std::println("Total errors:     {}", metric_.tot_error_cnt);
  std::println("Total reconns:    {}", metric_.tot_reconnects);
  std::println("Total retries:    {}", metric_.tot_retries);
  std::println("Total reinits:    {}", metric_.tot_reinits);
  std::println("Total polls:      {}", metric_.tot_polls);
  std::println("Hard fault:       {}", metric_.hard_fault);
  std::println();
}
  */
} // namespace tkk::s7