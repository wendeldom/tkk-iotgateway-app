// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/adapter/s7/context.hpp"
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <cstddef>
#include <memory>

namespace tkk::s7 {

enum class StateId : U8 {
  kInit,
  kConnect,
  kReconnect,
  kPrepare,
  kPoll,
  kFault,
};

class State : private NoCopyNoMove {
public:
  // copy/move assign/constr are deleted in the parent class
  virtual ~State() = default;

  [[nodiscard]] virtual auto Update(IAdapter &interface) -> std::unique_ptr<State> = 0;

  [[nodiscard]] auto GetDelay() const -> size_t { return delay_; }

  [[nodiscard]] auto GetState() const -> StateId { return state_id_; }

protected:
  [[nodiscard]] auto NextStateAfterError(Error &&error, IAdapter &interface)
      -> std::unique_ptr<State>;

  // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
  bool entry_{false};
  size_t delay_{0};
  StateId state_id_{StateId::kInit};
  StateMetric metric_{};
  // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)
};

class InitState final : public State {
public:
  InitState() { state_id_ = StateId::kInit; }

private:
  [[nodiscard]] auto Update(s7::IAdapter &interface) -> std::unique_ptr<State> override;
};

class ConnectState final : public State {
public:
  ConnectState() { state_id_ = StateId::kConnect; }

private:
  [[nodiscard]] auto Update(s7::IAdapter &interface) -> std::unique_ptr<State> override;
};

class ReconnectState final : public State {
public:
  ReconnectState() { state_id_ = StateId::kReconnect; }

private:
  [[nodiscard]] auto Update(s7::IAdapter &interface) -> std::unique_ptr<State> override;
};

class PrepareState final : public State {
public:
  PrepareState() { state_id_ = StateId::kPrepare; }

private:
  [[nodiscard]] auto Update(s7::IAdapter &interface) -> std::unique_ptr<State> override;
};

class PollState final : public State {
public:
  PollState() { state_id_ = StateId::kPoll; }

private:
  [[nodiscard]] auto Update(s7::IAdapter &interface) -> std::unique_ptr<State> override;
  size_t poll_cnt_{0};
};

class FaultState final : public State {
public:
  FaultState() { state_id_ = StateId::kFault; }

private:
  [[nodiscard]] auto Update(s7::IAdapter &interface) -> std::unique_ptr<State> override;
};

} // namespace tkk::s7
