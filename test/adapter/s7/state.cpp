// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/state.hpp"
#include "tkk/adapter/s7/context.hpp"
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/error.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <expected>

namespace {
auto FailWith(tkk::ErrA action) -> tkk::Result<void> {
  return std::unexpected(tkk::Error(tkk::ErrC::kConfigInvalid, action, ""));
}

class FakeAdapter final : public tkk::s7::IAdapter {
public:
  tkk::Result<void> fake_res;
  tkk::s7::AdapterCtx ctx{};
  tkk::s7::AdapterMetric metric{};

  [[nodiscard]] auto Init() -> tkk::Result<void> override { return fake_res; }

  [[nodiscard]] auto Connect() -> tkk::Result<void> override { return fake_res; }

  [[nodiscard]] auto Reconnect() -> tkk::Result<void> override { return fake_res; }

  [[nodiscard]] auto Prepare() -> tkk::Result<void> override { return fake_res; }

  [[nodiscard]] auto Poll() -> tkk::Result<void> override { return fake_res; }

  [[nodiscard]] auto GetContext() const -> const tkk::s7::AdapterCtx & override { return ctx; }

  void PublishStateMetric(tkk::s7::StateMetric new_metric) override { metric += new_metric; }
};

template <typename T> struct StateFixture {
  FakeAdapter fake_adp;
};
} // namespace

TEMPLATE_TEST_CASE_METHOD(StateFixture, "s7 state: return hard fault", "[s7][state]",
                          tkk::s7::InitState, tkk::s7::ConnectState, tkk::s7::ReconnectState,
                          tkk::s7::PrepareState, tkk::s7::PollState) {
  TestType state;
  this->fake_adp.fake_res = FailWith(tkk::ErrA::kFatal);
  this->fake_adp.ctx.name = "fake adapter";
  this->fake_adp.ctx.enabled = true;

  tkk::s7::State *state_ptr = &state;
  auto s1 = state_ptr->Update(this->fake_adp);
  CHECK(s1->GetState() == tkk::s7::StateId::kFault);
  CHECK(s1->Update(this->fake_adp) == nullptr);
  CHECK(this->fake_adp.metric.hard_fault);
}

TEMPLATE_TEST_CASE_METHOD(StateFixture, "s7 state: return reconnect", "[s7][state]",
                          tkk::s7::InitState, tkk::s7::ConnectState, tkk::s7::PrepareState,
                          tkk::s7::PollState) {
  TestType state;
  this->fake_adp.fake_res = FailWith(tkk::ErrA::kReconnect);
  this->fake_adp.ctx.name = "fake adapter";
  this->fake_adp.ctx.enabled = true;

  tkk::s7::State *state_ptr = &state;
  auto s1 = state_ptr->Update(this->fake_adp);
  CHECK(s1->GetState() == tkk::s7::StateId::kReconnect);
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
TEMPLATE_TEST_CASE_METHOD(StateFixture, "s7 state: publish metric after 5 reconns", "[s7][state]",
                          tkk::s7::ReconnectState) {
  TestType state;
  this->fake_adp.fake_res = FailWith(tkk::ErrA::kReconnect);
  this->fake_adp.ctx.name = "fake adapter";
  this->fake_adp.ctx.enabled = true;

  tkk::s7::State *state_ptr = &state;
  for (int i = 0; i < 5; i++) {
    REQUIRE(state_ptr->Update(this->fake_adp) == nullptr);
  }
  CHECK(this->fake_adp.metric.tot_reconnects == 5);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
TEMPLATE_TEST_CASE_METHOD(StateFixture, "s7 state: backoff delay", "[s7][state]",
                          tkk::s7::ReconnectState) {
  TestType state;
  this->fake_adp.fake_res = this->fake_adp.fake_res = FailWith(tkk::ErrA::kReconnect);
  this->fake_adp.ctx.name = "fake adapter";
  this->fake_adp.ctx.enabled = true;
  this->fake_adp.ctx.init_reconn_delay = 250;
  this->fake_adp.ctx.max_reconn_delay = 10000;
  this->fake_adp.ctx.reconn_factor = 2;

  tkk::s7::State *state_ptr = &state;
  for (int i = 0; i < 5; i++) {
    REQUIRE(state_ptr->Update(this->fake_adp) == nullptr);
  }
  CHECK(state_ptr->GetDelay() == 8000);

  REQUIRE(state_ptr->Update(this->fake_adp) == nullptr);
  CHECK(state_ptr->GetDelay() == 10000);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
TEMPLATE_TEST_CASE_METHOD(StateFixture, "s7 state: reach + stay in poll state if no error",
                          "[s7][state]", tkk::s7::InitState) {
  TestType state;
  this->fake_adp.fake_res = {};
  this->fake_adp.ctx.name = "fake adapter";
  this->fake_adp.ctx.enabled = true;

  tkk::s7::State *state_ptr = &state;
  REQUIRE(state_ptr->GetState() == tkk::s7::StateId::kInit);
  auto s1 = state_ptr->Update(this->fake_adp);
  REQUIRE(s1->GetState() == tkk::s7::StateId::kConnect);
  s1 = s1->Update(this->fake_adp);
  REQUIRE(s1->GetState() == tkk::s7::StateId::kPrepare);
  s1 = s1->Update(this->fake_adp);
  REQUIRE(s1->GetState() == tkk::s7::StateId::kPoll);

  for (int i = 0; i < 5; i++) {
    REQUIRE(s1->Update(this->fake_adp) == nullptr);
    REQUIRE(s1->GetState() == tkk::s7::StateId::kPoll);
  }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)