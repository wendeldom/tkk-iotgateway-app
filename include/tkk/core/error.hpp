// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "tkk/core/type.hpp"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace tkk {

class Error;

template <typename T> using Result = std::expected<T, Error>;

enum class ErrC : U8 {
  kUnknown,
  kFileError,
  kParseError,
  kConfigInvalid,
  kTimeout,
  kConnectionFailed,
  kReadError,
  kDpError,
  kBatchError,
  kDataBaseError,
  kDefaultParent,
};
enum class ErrA : U8 {
  kSkip,
  kRetry,
  kReconnect,
  kReinit,
  kFatal,
};

class Error {

private:
  using FieldValue = std::variant<int64_t, std::string, std::string_view>;

  struct ContextField {
    std::string_view key;
    FieldValue value;
  };

  static constexpr int kFieldLimit{6};
  static constexpr int kSubErrLimit{10};
  static constexpr int kResSpace{128};
  ErrC code_{ErrC::kUnknown};
  ErrA action_{ErrA::kFatal};
  std::string_view msg_;
  std::vector<ContextField> ctx_;
  std::vector<Error> sub_err_;
  size_t sub_err_cnt_{0};

public:
  // msg must be a static string_view or literal
  Error(ErrC code, ErrA action, std::string_view msg) noexcept
      : code_{code}, action_{action}, msg_{msg} {};

  [[nodiscard]] auto HasSubErr() const -> bool { return !(sub_err_.empty()); }

  [[nodiscard]] auto GetCode() const -> ErrC { return code_; }

  [[nodiscard]] auto GetAction() const -> ErrA { return action_; }

  [[nodiscard]] auto GetSubErrCnt() const -> size_t { return sub_err_cnt_; }

  [[nodiscard]] auto GetSubErrors() const -> const std::vector<Error> & { return sub_err_; }

  [[nodiscard]] auto FieldLimitExceeded() const -> bool { return ctx_.size() >= kFieldLimit; }

  [[nodiscard]] auto SubErrLimitExceeded() const -> bool { return sub_err_.size() >= kSubErrLimit; }

  [[nodiscard]] auto Fail() && -> std::unexpected<Error> {
    return std::unexpected<Error>(std::move(*this));
  }

  [[nodiscard]] auto Dump() && -> std::vector<Error>;
  void PushField(std::string_view key, const FieldValue &value);
  void Add(Error &&err);
  [[nodiscard]] auto Format() const -> std::string;

  template <std::integral T>
  [[nodiscard]] auto With(const std::string_view key, T value) && -> Error {
    PushField(key, static_cast<int64_t>(value));
    return std::move(*this);
  }

  // Only for static string_views and literals, never use for dynamic/temp
  // strings
  [[nodiscard]] auto WithView(const std::string_view key,
                              const std::string_view value) && -> Error {
    PushField(key, value);
    return std::move(*this);
  }

  // Use for dynamic/temporary strings etc.
  [[nodiscard]] auto WithStr(const std::string_view key, const std::string_view value) && -> Error {
    PushField(key, std::string{value});
    return std::move(*this);
  }
};

} // namespace tkk