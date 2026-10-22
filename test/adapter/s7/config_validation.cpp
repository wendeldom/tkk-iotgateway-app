// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/config_validation.hpp"
#include "helper/helper.hpp"
#include "tkk/adapter/s7/config_key.hpp"
#include "tkk/adapter/s7/type.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>

namespace key = tkk::s7::detail::key;

namespace {
struct ValidationFixture {
  tkk::Json cfg;

  ValidationFixture() { cfg = LoadJson(TKK_TEST_DATA_DIR "/valid/s7/base0.json"); }
};
} // namespace

TEST_CASE_METHOD(ValidationFixture, "s7 config: valid config", "[s7][config]") {
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE(res);
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: missing key", "[s7][config]") {
  REQUIRE(cfg.erase(key::kAdapter) == 1);
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {key::kAdapter, "missing", "key"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: missing dependent key", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(0).at(key::kTags).at(3).erase(key::kBit) == 1);
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {key::kBit, "missing", "key"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: type mismatch", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(1).at(key::kMode) = -1);
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {key::kMode, "type", "mismatch"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: number on limit", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(1).at(key::kTags).at(6).at(key::kOffset) =
                      tkk::s7::limit::kS7MaxOffset);
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE(res);
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: number on limit + 1", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(1).at(key::kTags).at(6).at(key::kOffset) =
                      tkk::s7::limit::kS7MaxOffset + 1);
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {key::kOffset, "bound"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: number on size_t max", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(1).at(key::kTags).at(7).at(key::kId) =
                      std::numeric_limits<size_t>::max());
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {"bound"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: name length on limit", "[s7][config]") {
  // Test with 128 characters
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(0).at(key::kTags).at(0).at(key::kName) =
                      "REQUIRE_NOTHROW(cfg.at(key::kRead).at(0).at(key::kTags).at(0).at"
                      "REQUIRE_NOTHROW(cfg.at(key::kRead).at(0).at(key::kTags).at(0).at");
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE(res);
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: name length on limit + 1", "[s7][config]") {
  // Test with 129 characters
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(0).at(key::kTags).at(0).at(key::kName) =
                      "REQUIRE_NOTHROW(cfg.at(key::kRead).at(0).at(key::kTags).at(0).at"
                      "REQUIRE_NOTHROW(cfg.at(key::kRead).at(0).at(key::kTags).at(0).at1");
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {"name", "bound"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: ip mismatch", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kConnection).at(key::kIp) = "192.164.alo123");
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {"invalid", "ip"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: invalid read mode", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(1).at(key::kMode) = "NUTTIN");
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {"invalid", "read mode"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: invalid data type", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(0).at(key::kTags).at(0).at(key::kDataType) =
                      "DoubleFloatBoolx10");
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {"invalid", "type"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: invlaid target", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(1).at(key::kTags).at(0).at(key::kTarget) = "France");
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {key::kTarget, "invalid"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: invalid area target", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(0).at(key::kAreaParams).at(key::kAreaTarget) = "India");
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {"invalid", "area", "target"});
}

TEST_CASE_METHOD(ValidationFixture, "s7 config: duplicate id", "[s7][config]") {
  REQUIRE_NOTHROW(cfg.at(key::kRead).at(1).at(key::kTags).at(5).at(key::kId) = 0U);
  auto res = tkk::s7::Validator(std::move(cfg)).Validate();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {key::kId, "duplicate"});
}