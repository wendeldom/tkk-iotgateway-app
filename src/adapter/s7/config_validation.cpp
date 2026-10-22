// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/config_validation.hpp"
#include "tkk/adapter/s7/config_key.hpp"
#include "tkk/adapter/s7/config_rule.hpp"
#include "tkk/adapter/s7/helper.hpp"
#include "tkk/core/type.hpp"
#include "tkk/core/util.hpp"
#include <array>
#include <cstddef>
#include <string_view>
#include <sys/types.h>
#include <utility>

namespace tkk::s7 {

namespace key = detail::key;

auto Validator::Validate() -> Result<Json> {

  static constexpr std::array kSteps = {&Validator::ValidateScheme};

  for (const auto step : kSteps) {
    if (auto res = (this->*step)(); !res) {
      return std::move(res).error().Fail();
    }
  }
  return config_;
}

auto Validator::ValidateScheme() -> Result<void> {

  auto res = CommonValidator{config_}.Validate();
  if (!res) {
    return res;
  }

  return {};
}

auto CommonValidator::Validate() -> Result<void> {
  static constexpr std::array kSteps = {
      &CommonValidator::ValidateAdapter,   &CommonValidator::ValidateConnection,
      &CommonValidator::ValidateReconnect, &CommonValidator::ValidatePolling,
      &CommonValidator::ValidateRead,
  };

  for (const auto step : kSteps) {
    if (auto res = (this->*step)(); !res) {
      return res;
    }
  }
  return {};
}

auto CommonValidator::ValidateAdapter() -> Result<void> {
  namespace key = detail::key;

  constexpr std::string_view kLocKey = key::kAdapter; // Local key used
  static constexpr std::array kReqKeys = {key::kId, key::kName, key::kEnabled};

  // Check for own key first
  if (auto res = RequiredKey(config_, kLocKey); !res) {
    return res;
  }
  // Assign the local json object
  const Json &loc_obj = config_.at(kLocKey);

  // Check for required keys next
  for (const auto key : kReqKeys) {
    if (auto res = RequiredKey(loc_obj, key); !res) {
      return std::move(res).error().WithView("required by", kLocKey).Fail();
    }
    if (auto res = CheckRule(loc_obj, key); !res) {
      return res;
    }
  }

  return {};
}

auto CommonValidator::ValidateConnection() -> Result<void> {
  namespace key = detail::key;

  constexpr std::string_view kLocKey = key::kConnection; // Local key used
  static constexpr std::array kReqKeys = {key::kIp, key::kRack, key::kSlot};

  // Check for own key first
  if (auto res = RequiredKey(config_, kLocKey); !res) {
    return res;
  }

  // Assign the local json object
  const Json &loc_obj = config_.at(kLocKey);

  // Check for required keys next
  for (const auto key : kReqKeys) {
    if (auto res = RequiredKey(loc_obj, key); !res) {
      return std::move(res).error().WithView("required by", kLocKey).Fail();
    }
    if (auto res = CheckRule(loc_obj, key); !res) {
      return res;
    }
  }
  return {};
}

auto CommonValidator::ValidateReconnect() -> Result<void> {
  namespace key = detail::key;

  constexpr std::string_view kLocKey = key::kReconnect; // Local key used
  static constexpr std::array kReqKeys = {key::kInitialDelayMs, key::kMaxDelayMs, key::kFactor};

  // Check for own key first
  if (auto res = RequiredKey(config_, kLocKey); !res) {
    return res;
  }

  // Assign the local json object
  const Json &loc_obj = config_.at(kLocKey);

  // Check for required keys next
  for (const auto key : kReqKeys) {
    if (auto res = RequiredKey(loc_obj, key); !res) {
      return std::move(res).error().WithView("required by", kLocKey).Fail();
    }
    if (auto res = CheckRule(loc_obj, key); !res) {
      return res;
    }
  }
  return {};
}

auto CommonValidator::ValidatePolling() -> Result<void> {
  namespace key = detail::key;

  constexpr std::string_view kLocKey = key::kPolling; // Local key used
  static constexpr std::array kReqKeys = {key::kIntervalMs};

  // Check for own key first
  if (auto res = RequiredKey(config_, kLocKey); !res) {
    return res;
  }

  // Assign the local json object
  const Json &loc_obj = config_.at(kLocKey);

  // Check for required keys next
  for (const auto key : kReqKeys) {
    if (auto res = RequiredKey(loc_obj, key); !res) {
      return std::move(res).error().WithView("required by", kLocKey).Fail();
    }
    if (auto res = CheckRule(loc_obj, key); !res) {
      return res;
    }
  }
  return {};
}

auto CommonValidator::ValidateRead() -> Result<void> {
  namespace key = detail::key;

  constexpr std::string_view kLocKey = key::kRead; // Local key used

  // Check for own key first
  if (auto res = RequiredKey(config_, kLocKey); !res) {
    return res;
  }

  // Assign the local json object
  const Json &loc_obj = config_.at(kLocKey);

  // Check for required keys next, iterate over all array items
  size_t idx{0};
  for (const auto &item : loc_obj) {
    idx++;
    if (auto res = RequiredKey(item, key::kMode); !res) {
      return std::move(res).error().With("at block", idx).Fail();
    }
    if (auto res = CheckRule(item, key::kMode); !res) {
      return std::move(res).error().With("at block", idx).Fail();
    }
    if (auto res = ValidateReadItem(item); !res) {
      return std::move(res).error().With("at block", idx).Fail();
    }
  }
  return {};
}

auto CommonValidator::ValidateReadItem(const Json &config) -> Result<void> {
  namespace key = detail::key;

  using enum ReadMode;

  Result<void> res{};

  const auto mode = detail::StringToReadMode(util::GetStr(config, key::kMode));
  switch (mode) {
  case kArea:
    res = AreaValidator{config, *this}.Validate();
    break;
  case kMulti:
    res = MultiValidator{config, *this}.Validate();
    break;
  default:
    break;
  }
  if (!res) {
    return res;
  }
  return {};
}

auto CommonValidator::CheckId(const int id) -> bool {
  auto [it, inserted] = seen_ids_.insert(id);
  return inserted;
}

auto AreaValidator::Validate() -> Result<void> {

  static constexpr std::array kSteps = {
      &AreaValidator::ValidateParams,
      &AreaValidator::ValidateTags,
  };

  for (const auto step : kSteps) {
    if (auto res = (this->*step)(); !res) {
      return res;
    }
  }
  return {};
}

auto AreaValidator::ValidateParams() -> Result<void> {

  constexpr std::string_view kLocKey = key::kAreaParams; // Local key used
  static constexpr std::array kKeys = {key::kAreaTarget, key::kOffset, key::kAmount};

  // Check for own key first
  if (auto res = RequiredKey(config_, kLocKey); !res) {
    return res;
  }

  // Assign the local json object
  const Json &loc_obj = config_.at(kLocKey);

  // Check for required keys next
  for (const auto key : kKeys) {
    if (auto res = RequiredKey(loc_obj, key); !res) {
      return std::move(res).error().WithView("required by", kLocKey).Fail();
    }
    if (auto res = CheckRule(loc_obj, key); !res) {
      return res;
    }
  }
  return {};
}

auto AreaValidator::ValidateTags() -> Result<void> {

  constexpr std::string_view kLocKey = key::kTags; // Local key used

  static constexpr std::array kReqKeys = {key::kId, key::kName, key::kOffset, key::kDataType};

  // Check for own key first
  if (auto res = RequiredKey(config_, kLocKey); !res) {
    return res;
  }

  // Assign the local json object
  const Json &loc_obj = config_.at(kLocKey);

  // Check for required keys next, iterate over all tags
  size_t idx{0};
  for (const auto &tag : loc_obj) {
    for (const auto key : kReqKeys) {
      if (auto res = RequiredKey(tag, key); !res) {
        return std::move(res).error().With("at tag", idx).Fail();
      }
      if (auto res = CheckRule(tag, key); !res) {
        return std::move(res).error().With("at tag", idx).Fail();
      }
    }
    if (!common_val_.CheckId(tag.at(key::kId))) {
      return Error(ErrC::kConfigInvalid, ErrA::kFatal, "duplicate id").With("at tag", idx).Fail();
    }
    idx++;
  }
  return {};
}

auto MultiValidator::Validate() -> Result<void> {

  static constexpr std::array kSteps = {
      &MultiValidator::ValidateTags,
  };

  for (const auto step : kSteps) {
    if (auto res = (this->*step)(); !res) {
      return res;
    }
  }
  return {};
}

auto MultiValidator::ValidateTags() -> Result<void> {

  constexpr std::string_view kLocKey = key::kTags; // Local key used
  static constexpr std::array kReqKeys = {key::kId, key::kName, key::kTarget};

  // Check for own key first
  if (auto res = RequiredKey(config_, kLocKey); !res) {
    return res;
  }

  // Assign the local json object
  const Json &loc_obj = config_.at(kLocKey);

  // Check for required keys next, iterate over all tags
  size_t idx{0};
  for (const auto &tag : loc_obj) {
    for (const auto key : kReqKeys) {
      if (auto res = RequiredKey(tag, key); !res) {
        return std::move(res).error().With("at tag", idx).Fail();
      }
      if (auto res = CheckRule(tag, key); !res) {
        return std::move(res).error().With("at tag", idx).Fail();
      }
    }
    if (!common_val_.CheckId(tag.at(key::kId))) {
      return Error(ErrC::kConfigInvalid, ErrA::kFatal, "duplicate id").With("at tag", idx).Fail();
    }
    idx++;
  }
  return {};
}

auto GetConnParams(const Json &config) -> S7Connection {

  S7Connection conn{};

  const auto &conn_block = config.at(key::kConnection);

  conn.ip = conn_block.at(key::kIp);
  conn.rack = conn_block.at(key::kRack);
  conn.slot = conn_block.at(key::kSlot);

  return conn;
}

auto GetAdapterParams(const Json &config) -> Result<AdapterInfo> {
  constexpr std::array kReqKeys1 = {key::kProtocol, key::kAdapter};
  constexpr std::array kReqKeys2 = {key::kId, key::kName};

  for (const auto key : kReqKeys1) {
    if (auto res = s7::RequiredKey(config, key); !res) {
      return std::move(res).error().Fail();
    }
    if (auto res = s7::CheckRule(config, key); !res) {
      return std::move(res).error().Fail();
    }
  }

  const auto &adap_block = config.at(key::kAdapter);

  for (const auto key : kReqKeys2) {
    if (auto res = s7::RequiredKey(adap_block, key); !res) {
      return std::move(res).error().Fail();
    }
    if (auto res = s7::CheckRule(adap_block, key); !res) {
      return std::move(res).error().Fail();
    }
  }

  return AdapterInfo{
      .id = util::GetUint(adap_block, key::kId),
      .name = util::GetStr(adap_block, key::kName),
      .protocol = util::StringToProtocol(util::GetStr(config, key::kProtocol)),
  };
}
} // namespace tkk::s7