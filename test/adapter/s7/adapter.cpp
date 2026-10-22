// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/adapter.hpp"
#include "helper/helper.hpp"
#include "tkk/adapter/s7/client.hpp"
#include "tkk/adapter/s7/config_key.hpp"
#include "tkk/adapter/s7/context.hpp"
#include "tkk/adapter/s7/state.hpp"
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <s7_micro_client.h>
#include <s7_types.h>

namespace key = tkk::s7::detail::key;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

// Client fake
namespace {

constexpr size_t kPdu240 = 240;
constexpr size_t kPdu480 = 480;
constexpr size_t kPdu960 = 960;

class Fake7Client final : public ISnap7Client {
public:
  int default_res{0};
  // Queues not thread safe, fill before moving into adapter
  std::deque<int> connect_results;
  std::deque<int> area_results;
  std::deque<int> multi_results;
  std::deque<int> multi_item_results;
  std::atomic_int connect_calls;
  std::atomic_int area_calls;
  std::atomic<void *> last_area_ptr;
  std::atomic<void *> last_multi_ptr;
  std::atomic_int multi_calls;
  std::atomic_int pdu{-1};
  std::atomic_bool connected{false};
  tkk::s7::S7Connection last_conn_params{};

  inline static std::mutex mtx_;

  [[nodiscard]] auto ConnectTo(const char *rem_address, int rack, int slot) -> int override;

  auto Reset(bool do_reconnect) -> int override { return default_res; }

  auto Disconnect() -> int override { return default_res; }

  [[nodiscard]] auto ReadArea(int area, int db_number, int start, int amount, int word_len,
                              void *p_usr_data) -> int override;
  [[nodiscard]] auto ReadMultiVars(PS7DataItem item, int items_count) -> int override;

  [[nodiscard]] auto GetPduLength() -> int override { return pdu; }

  [[nodiscard]] auto Connected() -> bool override { return connected; }

  [[nodiscard]] auto GetLastConnParams() const -> tkk::s7::S7Connection {
    std::scoped_lock lock(mtx_);
    return last_conn_params;
  }
};

auto Fake7Client::ConnectTo(const char *rem_address, int rack, int slot) -> int {
  {
    std::scoped_lock lock(mtx_);
    last_conn_params.ip = rem_address;
    last_conn_params.rack = rack;
    last_conn_params.slot = slot;
  }

  connect_calls++;
  if (connect_results.empty()) {
    return default_res;
  }
  auto res = connect_results.front();
  connect_results.pop_front();
  return res;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto Fake7Client::ReadArea(int area, int db_number, int start, int amount, int word_len,
                           void *p_usr_data) -> int {
  area_calls++;
  last_area_ptr = p_usr_data;
  if (area_results.empty()) {
    return default_res;
  }
  auto res = area_results.front();
  area_results.pop_front();
  return res;
}

auto Fake7Client::ReadMultiVars(PS7DataItem item, int items_count) -> int {
  multi_calls++;
  last_multi_ptr = item;
  auto view = std::span(item, items_count);
  for (auto &data_item : view) {
    if (multi_item_results.empty()) {
      data_item.Result = default_res;
      continue;
    }
    data_item.Result = multi_item_results.front();
    multi_item_results.pop_front();
  }

  if (multi_results.empty()) {
    return default_res;
  }
  auto res = multi_results.front();
  multi_results.pop_front();
  return res;
}
} // namespace

// State fake
namespace {
class FakeState final : public tkk::s7::State {
public:
  std::atomic_int update_calls;
  tkk::s7::AdapterCtx last_adp_ctx{};
  tkk::Result<void> last_func_result;
  tkk::s7::StateMetric metric;
  inline static std::mutex ctx_mtx_;
  inline static std::mutex res_mtx_;
  inline static std::mutex mtc_mtx_;

  // Queues not thread safe, fill before moving into adapter
  std::deque<tkk::Result<void> (tkk::s7::IAdapter::*)()> iface_functions;

  // This function is used if the adapter is running in its own thread
  [[nodiscard]] auto Update(tkk::s7::IAdapter &interface) -> std::unique_ptr<State> override {
    update_calls++;
    {
      std::scoped_lock lock(ctx_mtx_);
      last_adp_ctx = interface.GetContext();
    }
    if (!iface_functions.empty()) {
      std::scoped_lock lock(res_mtx_);
      auto next_func = iface_functions.front();
      last_func_result = (interface.*next_func)();
      iface_functions.pop_front();
    } else {
      delay_ = 60000; // set delay to 60s after no more actions need to be done
                      // (end of test)
    }
    interface.PublishStateMetric(metric);
    return nullptr;
  }

  [[nodiscard]] auto GetLastAdpCtx() const -> tkk::s7::AdapterCtx {
    std::scoped_lock lock(ctx_mtx_);
    return last_adp_ctx;
  }

  [[nodiscard]] auto GetLastFuncRes() const -> tkk::Result<void> {
    std::scoped_lock lock(res_mtx_);
    return last_func_result;
  }

  void WriteStateMetric(tkk::s7::StateMetric new_metric) {
    std::scoped_lock lock(mtc_mtx_);
    metric = new_metric;
  }
};
} // namespace

// Fixture
namespace {
struct AdapterFixture {
  tkk::Json cfg;
  std::unique_ptr<Fake7Client> client;
  std::unique_ptr<FakeState> state;
  Fake7Client *client_ptr;
  FakeState *state_ptr;

  AdapterFixture()
      : client{std::make_unique<Fake7Client>()}, state{std::make_unique<FakeState>()},
        client_ptr(client.get()), state_ptr(state.get()) {}

  void Load(const std::string &file) {
    cfg = LoadJson(TKK_TEST_DATA_DIR "/valid/s7/" + file + ".json");
  }

  static auto MakeAdapter(tkk::Json &&config, size_t id, const std::string &name,
                          std::unique_ptr<Fake7Client> client, std::unique_ptr<FakeState> state)
      -> std::unique_ptr<tkk::s7::Adapter> {

    return std::make_unique<tkk::s7::Adapter>(std::move(config), id, name, std::move(client),
                                              std::move(state));
  }
};
} // namespace

TEST_CASE_METHOD(AdapterFixture, "s7 adapter: correct conn params", "[s7][adapter]") {
  Load("base2");
  // Setup fake client
  client->pdu = kPdu240;
  // Dont start the adapter thread, just manually call the interface functions
  // to have synced results
  auto adp = MakeAdapter(std::move(cfg), 4, "fake adapter", std::move(client), std::move(state));

  // Cast to IAdapter (for interface functions)
  tkk::s7::IAdapter &iface = *adp;
  tkk::Result<void> res;
  res = iface.Init();
  REQUIRE(res);
  res = iface.Connect();
  REQUIRE(res);

  // Finally check for correct params
  auto conn_params = client_ptr->GetLastConnParams();
  CHECK(conn_params.ip == "192.168.10.83");
  CHECK(conn_params.rack == 5);
  CHECK(conn_params.slot == 3);
}

TEST_CASE_METHOD(AdapterFixture, "s7 adapter: correct context params", "[s7][adapter]") {
  Load("base2");
  // Setup fake client
  client->pdu = kPdu240;
  // Dont start the adapter thread, just manually call the interface functions
  // to have synced results
  auto adp = MakeAdapter(std::move(cfg), 4, "fake adapter", std::move(client), std::move(state));

  // Cast to IAdapter (for interface functions)
  tkk::s7::IAdapter &iface = *adp;
  tkk::Result<void> res;
  res = iface.Init();
  REQUIRE(res);
  res = iface.Connect();
  REQUIRE(res);
  res = iface.Prepare();
  REQUIRE(res);

  auto context = iface.GetContext();
  CHECK(context.enabled);
  CHECK(context.id == 4);
  CHECK(context.name == "fake adapter");
  CHECK(context.poll_interval == 250);
  CHECK(context.init_reconn_delay == 250);
  CHECK(context.max_reconn_delay == 2100);
  CHECK(context.reconn_factor == 2);
}

TEST_CASE_METHOD(AdapterFixture, "s7 adapter: conn failure", "[s7][adapter]") {
  Load("base2");
  // Setup fake client
  client->pdu = kPdu240;
  client->connect_results = {
      WSAETIMEDOUT,
      WSAECONNREFUSED,
  };
  // Dont start the adapter thread, just manually call the interface functions
  // to have synced results
  auto adp = MakeAdapter(std::move(cfg), 4, "fake adapter", std::move(client), std::move(state));

  // Cast to IAdapter (for interface functions)
  tkk::s7::IAdapter &iface = *adp;
  tkk::Result<void> res;

  res = iface.Init();
  REQUIRE(res);
  res = iface.Connect();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConnectionFailed);
  res = iface.Reconnect();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConnectionFailed);
  res = iface.Reconnect();
  REQUIRE(res);
}

TEST_CASE_METHOD(AdapterFixture, "s7 adapter: reconn, same pdu", "[s7][adapter]") {
  Load("base2");
  // setup fake client
  client->pdu = kPdu240;
  client->area_results = {
      errCliInvalidPlcAnswer,
  };
  // dont start the adapter thread, just manually call the interface functions
  // to have synced results
  auto adp = MakeAdapter(std::move(cfg), 4, "fake adapter", std::move(client), std::move(state));

  // cast to IAdapter (for interface functions)
  tkk::s7::IAdapter &iface = *adp;
  tkk::Result<void> res;

  res = iface.Init();
  REQUIRE(res);
  res = iface.Connect();
  REQUIRE(res);
  res = iface.Prepare();
  REQUIRE(res);
  res = iface.Poll();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kReadError, {"invalid", "plc", "answer"});
  // write away ptr for area read
  const void *old_ptr = client_ptr->last_area_ptr;
  res = iface.Reconnect();
  REQUIRE(res);
  res = iface.Prepare();
  REQUIRE(res);
  res = iface.Poll();
  REQUIRE(res);
  // write away ptr for area read
  const void *new_ptr = client_ptr->last_area_ptr;

  // should be same, no new batch creation, bcs. pdu is same as before
  REQUIRE(old_ptr == new_ptr);
}

TEST_CASE_METHOD(AdapterFixture, "s7 adapter: reconn, diff. pdu", "[s7][adapter]") {
  Load("base2");
  // setup fake client
  client->pdu = kPdu240;
  client->area_results = {
      errCliInvalidPlcAnswer,
  };
  // dont start the adapter thread, just manually call the interface functions
  // to have synced results
  auto adp = MakeAdapter(std::move(cfg), 4, "fake adapter", std::move(client), std::move(state));

  // cast to IAdapter (for interface functions)
  tkk::s7::IAdapter &iface = *adp;
  tkk::Result<void> res;

  res = iface.Init();
  REQUIRE(res);
  res = iface.Connect();
  REQUIRE(res);
  res = iface.Prepare();
  REQUIRE(res);
  res = iface.Poll();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kReadError, {"invalid", "plc", "answer"});
  // write away ptr for area read
  const void *old_ptr = client_ptr->last_area_ptr;
  // set new pdu size
  client_ptr->pdu = kPdu480;
  res = iface.Reconnect();
  REQUIRE(res);
  res = iface.Prepare();
  REQUIRE(res);
  res = iface.Poll();
  REQUIRE(res);
  // write away ptr for area read
  const void *new_ptr = client_ptr->last_area_ptr;

  // should NOT be same, new batch creation, bcs. pdu is another as before
  REQUIRE(old_ptr != new_ptr);
}

TEST_CASE_METHOD(AdapterFixture, "s7 adapter: reconn, big item, smaller pdu", "[s7][adapter]") {
  Load("base2");
  // setup fake client
  client->pdu = kPdu480;
  client->area_results = {
      errCliInvalidPlcAnswer,
  };

  // set key to a datatype size that cannot be handled with pdu 240
  cfg.at(key::kRead).at(1).at(key::kTags).at(15).at(key::kLength) = 128U;

  // dont start the adapter thread, just manually call the interface functions
  // to have synced results
  auto adp = MakeAdapter(std::move(cfg), 4, "fake adapter", std::move(client), std::move(state));

  // cast to IAdapter (for interface functions)
  tkk::s7::IAdapter &iface = *adp;
  tkk::Result<void> res;

  res = iface.Init();
  REQUIRE(res);
  res = iface.Connect();
  REQUIRE(res);
  res = iface.Prepare();
  REQUIRE(res);
  res = iface.Poll();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kReadError, {"invalid", "plc", "answer"});
  // set new pdu size
  client_ptr->pdu = kPdu240;
  res = iface.Reconnect();
  REQUIRE(res);
  res = iface.Prepare();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kBatchError, {"too", "big", "pdu"});
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)