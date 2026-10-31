// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/core/error.hpp"
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

namespace tkk {
namespace {
[[nodiscard]] constexpr auto ErrCtoString(const ErrC errc) -> std::string_view {
  using enum ErrC;
  switch (errc) {
  case kUnknown:
    return "unknown error";
  case kFileError:
    return "file error";
  case kParseError:
    return "parse error";
  case kConfigInvalid:
    return "config error";
  case kConnectionFailed:
    return "connection error";
  case kTimeout:
    return "timeout";
  case kReadError:
    return "read error";
  case kDpError:
    return "data point error";
  case kBatchError:
    return "batch creation error";
  case kDataBaseError:
    return "database error";
  case kDefaultParent:
    return "";
  }
  return "unknown";
}

} // namespace

void Error::PushField(const std::string_view key, const FieldValue &value) {
  ctx_.reserve(kFieldLimit);
  if (!FieldLimitExceeded()) {
    ctx_.emplace_back(key, value);
  }
}

auto Error::Add(Error &&err) -> void {
  sub_err_.reserve(kSubErrLimit);
  auto children = std::move(err.sub_err_);
  err.sub_err_.clear();
  sub_err_cnt_ += children.size();

  if (err.code_ != ErrC::kDefaultParent) {
    sub_err_cnt_++;
    // Make sure the most singf. error reaction actually gets done, even when
    // the error itself does not fit into sub error vector
    err.action_ > action_ ? action_ = err.action_ : action_;
    if (!SubErrLimitExceeded()) {
      sub_err_.push_back(std::move(err));
    }
  }
  for (auto &error : children) {
    // Make sure the most singf. error reaction actually gets done, even when
    // the error itself does not fit into sub error vector
    error.action_ > action_ ? action_ = error.action_ : action_;
    if (!SubErrLimitExceeded()) {
      sub_err_.push_back(std::move(error));
    }
  }
}

[[nodiscard]] auto Error::Format() const -> std::string {
  std::string err_text;
  err_text.reserve(kResSpace);

  std::format_to(std::back_inserter(err_text), "{}: {} ", ErrCtoString(code_), msg_);

  for (const auto &field : ctx_) {
    if (const auto *val = std::get_if<std::string_view>(&field.value)) {
      std::format_to(std::back_inserter(err_text), "{}['{}'] ", field.key, *val);
    }
    if (const auto *val = std::get_if<std::string>(&field.value)) {
      std::format_to(std::back_inserter(err_text), "{}['{}'] ", field.key, *val);
    }
    if (const auto *val = std::get_if<int64_t>(&field.value)) {
      std::format_to(std::back_inserter(err_text), "{}[{}] ", field.key, *val);
    }
  }
  return err_text;
}

[[nodiscard]] auto Error::Dump() && -> std::vector<Error> {
  std::vector<Error> errors{};
  if (code_ != ErrC::kDefaultParent) {
    errors.reserve(sub_err_cnt_ + 1);
    errors.emplace_back(*this);
    errors.insert(errors.end(), std::make_move_iterator(sub_err_.begin()),
                  std::make_move_iterator(sub_err_.end()));
    sub_err_.clear();
    return errors;
  }
  // Dont count default parent as error and just leave it behind :o
  std::swap(errors, this->sub_err_);
  sub_err_.clear();
  return errors;
}
} // namespace tkk