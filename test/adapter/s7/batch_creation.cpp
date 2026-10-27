// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/batch_creation.hpp"
#include "helper/helper.hpp"
#include "tkk/adapter/s7/config_key.hpp"
#include "tkk/adapter/s7/config_validation.hpp"
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <s7_types.h>
#include <utility>

namespace key = tkk::s7::detail::key;

namespace {
constexpr size_t kPdu240 = 240;
constexpr size_t kPdu480 = 480;
constexpr size_t kPdu960 = 960;

struct CreatorFixture {
  tkk::Json cfg;
  tkk::Json config;

  void Load(const std::string &file) {
    cfg = LoadJson(TKK_TEST_DATA_DIR "/valid/s7/" + file + ".json");
  }

  void Validate() { REQUIRE_NOTHROW(config = *tkk::s7::Validator(std::move(cfg)).Validate()); }

  void Generate(const std::string &file) {
    Load(file);
    Validate();
  }
};
} // namespace

TEST_CASE_METHOD(CreatorFixture, "s7 batch: valid common fields, pdu 240", "[s7][batch]") {
  Generate("base1");
  auto res = tkk::s7::Creator(config, kPdu240).Create();
  REQUIRE(res);
  auto item = std::move(*res);
  CHECK(item.ctx.enabled);
  CHECK(item.ctx.init_reconn_delay == 2000);
  CHECK(item.ctx.max_reconn_delay == 30000);
  CHECK(item.ctx.reconn_factor == 2);
  CHECK(item.ctx.poll_interval == 1000);
  CHECK(item.total_batch == 3);
  CHECK(item.total_dp == 40);
  CHECK(item.total_read_size == 170);
}

TEST_CASE_METHOD(CreatorFixture, "s7 batch: valid multi, pdu 240", "[s7][batch]") {
  Generate("base1");
  auto res = tkk::s7::Creator(config, kPdu240).Create();
  REQUIRE(res);
  auto item = std::move(*res);

  // Checks for a multi read batch
  auto &batch1 = item.batches[1];
  // Check that all containers have at least size for item_count
  auto lo = std::min({
      batch1.snap7_data_items.capacity(),
      batch1.data_points.capacity(),
      batch1.tags.capacity(),
  });
  CHECK(lo >= batch1.header.item_count);
  CHECK(batch1.buffer.size() == batch1.header.read_buffer_size);

  CHECK(batch1.header.request_pdu_size == 144);
  CHECK(batch1.header.response_pdu_size == 86);
  CHECK(batch1.header.batch_no == 1);
  CHECK(batch1.header.mode == tkk::s7::ReadMode::kMulti);
}

TEST_CASE_METHOD(CreatorFixture, "s7 batch: valid area, pdu 240", "[s7][batch]") {
  Generate("base1");
  auto res = tkk::s7::Creator(config, kPdu240).Create();
  REQUIRE(res);
  auto item = std::move(*res);

  // Checks for a area read batch
  auto &batch2 = item.batches[2];

  // Check that all containers have at least size for item_count
  auto lo = std::min({
      batch2.snap7_data_items.capacity(),
      batch2.data_points.capacity(),
      batch2.tags.capacity(),
  });
  CHECK(lo >= batch2.header.item_count);
  CHECK(batch2.buffer.size() == batch2.header.read_buffer_size);

  // Req and Resp pdu size dont matter in area mode, snap7 splits it up by
  // itself
  CHECK(batch2.header.request_pdu_size == 0);
  CHECK(batch2.header.response_pdu_size == 0);

  CHECK(batch2.header.batch_no == 2);
  CHECK(batch2.header.number == 1);
  CHECK(batch2.header.offset == 2);
  CHECK(batch2.header.target == S7AreaDB);
  CHECK(batch2.header.mode == tkk::s7::ReadMode::kArea);
}

TEST_CASE_METHOD(CreatorFixture, "s7 batch: valid multi, pdu 480", "[s7][batch]") {
  Generate("base1");
  auto res = tkk::s7::Creator(config, kPdu960).Create();
  REQUIRE(res);
  auto item = std::move(*res);
  CHECK(item.total_batch == 2);
  CHECK(item.total_dp == 40);
  CHECK(item.total_read_size == 170);

  // Checks for a multi read batch
  auto &batch0 = item.batches[0];
  // Check that all containers have at least size for item_count
  auto lo = std::min({
      batch0.snap7_data_items.capacity(),
      batch0.data_points.capacity(),
      batch0.tags.capacity(),
  });
  CHECK(lo >= batch0.header.item_count);
  CHECK(batch0.buffer.size() == batch0.header.read_buffer_size);

  CHECK(batch0.header.request_pdu_size == 372);
  CHECK(batch0.header.response_pdu_size == 294);
  CHECK(batch0.header.batch_no == 0);
  CHECK(batch0.header.mode == tkk::s7::ReadMode::kMulti);
}

TEST_CASE_METHOD(CreatorFixture, "s7 batch: response > request, pdu 960", "[s7][batch]") {
  Generate("base3");
  auto res = tkk::s7::Creator(config, kPdu960).Create();
  REQUIRE(res);
  auto item = std::move(*res);
  CHECK(item.total_batch == 2);
  CHECK(item.total_dp == 30);
  CHECK(item.total_read_size == 1320);
}

TEST_CASE_METHOD(CreatorFixture, "s7 batch: response = pdu limit, pdu 480", "[s7][batch]") {
  Generate("base4");
  auto res = tkk::s7::Creator(config, kPdu480).Create();
  REQUIRE(res);
  auto item = std::move(*res);
  CHECK(item.total_batch == 1);
  CHECK(item.total_dp == 4);
  CHECK(item.batches[0].header.response_pdu_size == 480);
}

TEST_CASE_METHOD(CreatorFixture, "s7 batch: response = pdu limit + 1, pdu 480 ", "[s7][batch]") {
  Load("base4");

  REQUIRE_NOTHROW([&]() -> void {
    auto &tags = cfg.at(key::kRead).at(0).at(key::kTags);
    tags.at(3).at(key::kLength) = 59U;
  }());

  Validate();
  auto res = tkk::s7::Creator(config, kPdu480).Create();
  REQUIRE(res);
  auto item = std::move(*res);
  CHECK(item.total_batch == 2);
  CHECK(item.total_dp == 4);
  CHECK(item.batches[0].header.response_pdu_size == 416);
  CHECK(item.batches[1].header.response_pdu_size == 80);
}

TEST_CASE_METHOD(CreatorFixture, "s7 batch: item size = pdu + 1, pdu 240", "[s7][batch]") {
  constexpr uint32_t kHdrSizeRCV = 18;
  constexpr uint32_t kStrHdr = 2;
  Load("base4");

  REQUIRE_NOTHROW([&]() -> void {
    auto &tags = cfg.at(key::kRead).at(0).at(key::kTags);
    tags.at(0).at(key::kLength) = ((kPdu240 - kHdrSizeRCV - kStrHdr)) + 1U;
  }());

  Validate();
  auto res = tkk::s7::Creator(config, kPdu240).Create();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kBatchError, {"item", "pdu"});
}

TEST_CASE_METHOD(CreatorFixture, "s7 batch: tag offset on area limit", "[s7][batch]") {
  Load("base0");
  REQUIRE_NOTHROW([&]() -> void {
    auto &tags = cfg.at(key::kRead).at(0).at(key::kTags);
    tags.at(8).at(key::kOffset) = 18U;
  }());

  Validate();
  auto res = tkk::s7::Creator(config, kPdu240).Create();
  REQUIRE(res);
}

TEST_CASE_METHOD(CreatorFixture, "s7 batch: tag offset on area limit + 1", "[s7][batch]") {
  Load("base0");
  REQUIRE_NOTHROW([&]() -> void {
    auto &tags = cfg.at(key::kRead).at(0).at(key::kTags);
    tags.at(8).at(key::kOffset) = 19U;
  }());

  Validate();
  auto res = tkk::s7::Creator(config, kPdu240).Create();
  REQUIRE_FALSE(res);
  CheckError(std::move(res).error(), tkk::ErrC::kBatchError, {"offset", "area"});
}