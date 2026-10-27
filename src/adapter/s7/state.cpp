// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/state.hpp"
#include "tkk/adapter/s7/context.hpp"
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/util.hpp"
#include <cstddef>
#include <cstdio>
#include <memory>
#include <utility>

namespace tkk::s7 {

auto InitState::Update(IAdapter &interface) -> std::unique_ptr<State> {
  // const AdapterCtx &adp_ctx = interface.GetContext();

  if (!entry_) {
    entry_ = true;
  }

  auto res = interface.Init();
  if (!res) {
    return NextStateAfterError(std::move(res).error(), interface);
  }

  interface.PublishStateMetric(metric_);
  return std::make_unique<ConnectState>();
}

auto ConnectState::Update(IAdapter &interface) -> std::unique_ptr<State> {
  const AdapterCtx &adp_ctx = interface.GetContext();

  if (!entry_) {
    entry_ = true;
  }
  auto res = interface.Connect();
  if (!res) {
    delay_ = adp_ctx.init_reconn_delay;
    return NextStateAfterError(std::move(res).error(), interface);
  }

  interface.PublishStateMetric(metric_);
  return std::make_unique<PrepareState>();
}

auto ReconnectState::Update(IAdapter &interface) -> std::unique_ptr<State> {
  const AdapterCtx &adp_ctx = interface.GetContext();

  if (!entry_) {
    delay_ = adp_ctx.init_reconn_delay;
    entry_ = true;
  }

  auto res = interface.Reconnect();
  metric_.reconnects++;

  interface.PublishStateMetric(metric_);
  metric_ = StateMetric{};

  if (!res) {
    if ((delay_ * adp_ctx.reconn_factor) <= adp_ctx.max_reconn_delay) {
      delay_ *= adp_ctx.reconn_factor;
    } else {
      delay_ = adp_ctx.max_reconn_delay;
    }
    return NextStateAfterError(std::move(res).error(), interface);
  }

  delay_ = 0;
  return std::make_unique<PrepareState>();
}

auto PrepareState::Update(IAdapter &interface) -> std::unique_ptr<State> {
  // const AdapterCtx &adp_ctx = interface.GetContext();

  if (!entry_) {
    entry_ = true;
  }

  auto res = interface.Prepare();

  if (!res) {
    return NextStateAfterError(std::move(res).error(), interface);
  }

  interface.PublishStateMetric(metric_);
  return std::make_unique<PollState>();
}

auto PollState::Update(IAdapter &interface) -> std::unique_ptr<State> {
  const AdapterCtx &adp_ctx = interface.GetContext();

  if (!entry_) {
    delay_ = adp_ctx.poll_interval;
    entry_ = true;
  }

  if (!adp_ctx.enabled) {
    return nullptr;
  } // Stay in Poll state if polling is disabled, but dont poll
  auto res = interface.Poll();
  metric_.polls++;

  interface.PublishStateMetric(metric_);
  metric_ = StateMetric{};

  if (!res) {
    return NextStateAfterError(std::move(res).error(), interface);
  }

  return nullptr;
}

auto FaultState::Update(IAdapter &interface) -> std::unique_ptr<State> {

  if (!entry_) {
    constexpr size_t kDefaultDelay = 30000;
    delay_ = kDefaultDelay;
    metric_.hard_fault = true;
    interface.PublishStateMetric(metric_);
    entry_ = true;
  }
  return nullptr;
}

auto State::NextStateAfterError(Error &&err, IAdapter &interface) -> std::unique_ptr<State> {
  const AdapterCtx &adp_ctx = interface.GetContext();

  auto &&errors = std::move(err).Dump();
  // Get the action of the first error (parent), which is the most sign. action
  // of all avail. errors
  auto action = errors.front().GetAction();
  metric_.error_cnt += errors.size();
  log::LogErrors(std::move(errors), adp_ctx.name);
  errors.clear(); // Empty but valid after move

  switch (action) {
  case ErrA::kSkip:
    return nullptr;
  case ErrA::kFatal:
    interface.PublishStateMetric(metric_);
    return std::make_unique<FaultState>();
  case ErrA::kReconnect:
    if (state_id_ == StateId::kReconnect) {
      return nullptr;
    }
    delay_ = adp_ctx.init_reconn_delay;
    interface.PublishStateMetric(metric_);
    return std::make_unique<ReconnectState>();
  case ErrA::kRetry:
    metric_.retries++;
    return nullptr;
  case ErrA::kReinit:
    metric_.reinits++;
    interface.PublishStateMetric(metric_);
    return std::make_unique<FaultState>();
  }

  return std::make_unique<FaultState>();
}
} // namespace tkk::s7