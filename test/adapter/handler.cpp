// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/handler.hpp"
#include "helper/helper.hpp"

namespace {
constexpr std::string_view kProtocol = "protocol";
constexpr std::string_view kAdapter = "adapter";
constexpr std::string_view kId = "id";
constexpr std::string_view kName = "name";

constexpr int kTotalValidConfigs = 3;
} // namespace

struct HandlerFixture {
  std::filesystem::path dir;

  HandlerFixture() {
    dir = std::filesystem::temp_directory_path() / "tkk_test_handler";
    std::filesystem::create_directories(dir);
    std::filesystem::create_directories(dir / "s7");
  }

  ~HandlerFixture() { RemoveTemp(); }

  void WriteToS7Dir(const std::string &file, const nlohmann::json &cfg) const {
    std::ofstream(dir / "s7" / (file + ".json")) << cfg.dump(2);
  }

  void RawToS7Dir(const std::string &file,
                  const std::string &text) const { // for garbage syntax
    std::ofstream(dir / "s7" / (file + ".json")) << text;
  }

  void RemoveTemp() const {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  }
};

TEST_CASE_METHOD(HandlerFixture, "handler: directory not found", "[handler]") {
  tkk::adapter::Handler handler{dir / "abc"};
  auto res = handler.Start();
  REQUIRE_FALSE(res); //  Error
  CheckError(std::move(res).error(), tkk::ErrC::kFileError, {"directory", "found"});
}

TEST_CASE_METHOD(HandlerFixture, "handler: 3 configs accepted", "[handler]") {
  std::filesystem::copy_file(TKK_TEST_DATA_DIR "/valid/s7/base0.json", dir / "s7/temp0.json");
  std::filesystem::copy_file(TKK_TEST_DATA_DIR "/valid/s7/base1.json", dir / "s7/temp1.json");
  std::filesystem::copy_file(TKK_TEST_DATA_DIR "/valid/s7/base3.json", dir / "s7/temp3.json");
  tkk::adapter::Handler handler{dir};
  const auto res = handler.Start();
  REQUIRE(res); // No Error
  const auto status = handler.GetStatus();
  CHECK(status.tot_restarts == 0);
  CHECK(status.adapter_count == kTotalValidConfigs);
  CHECK(status.active_ids.size() == kTotalValidConfigs);
}

TEST_CASE_METHOD(HandlerFixture, "handler: 0 configs accepted", "[handler]") {
  tkk::adapter::Handler handler{dir};
  const auto res = handler.Start();
  REQUIRE(res); // No Error
  const auto status = handler.GetStatus();
  CHECK(status.tot_restarts == 0);
  CHECK(status.adapter_count == 0);
  CHECK(status.active_ids.size() == 0);
}

TEST_CASE_METHOD(HandlerFixture, "handler: 1 garbage file rejected", "[handler]") {
  RawToS7Dir("garbage_stx", "Invalid ass syntax wtf");
  tkk::adapter::Handler handler{dir};
  auto res = handler.Start();
  REQUIRE_FALSE(res); //  Error
  CheckError(std::move(res).error(), tkk::ErrC::kParseError);
}

TEST_CASE_METHOD(HandlerFixture, "handler: 1/2 configs accepted", "[handler]") {
  const auto cfg = LoadJson(TKK_TEST_DATA_DIR "/valid/s7/base0.json");
  WriteToS7Dir("valid_cfg", cfg);
  RawToS7Dir("garbage_stx", "Invalid ass syntax wtf");
  tkk::adapter::Handler handler{dir};
  auto res = handler.Start();
  REQUIRE_FALSE(res); //  Error
  CheckError(std::move(res).error(), tkk::ErrC::kParseError);
  const auto status = handler.GetStatus();
  CHECK(status.adapter_count == 1);
  CHECK(status.active_ids.size() == 1);
}

TEST_CASE_METHOD(HandlerFixture, "handler: duplicate id, 1 config rejected", "[handler]") {

  const auto cfg = LoadJson(TKK_TEST_DATA_DIR "/valid/s7/base0.json");
  WriteToS7Dir("valid_cfg1", cfg);
  WriteToS7Dir("valid_cfg2", cfg);
  tkk::adapter::Handler handler{dir};
  auto res = handler.Start();
  REQUIRE_FALSE(res); //  Error
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {"duplicate", "id"});
  const auto status = handler.GetStatus();
  CHECK(status.adapter_count == 1);
  CHECK(status.active_ids.size() == 1);
}

TEST_CASE_METHOD(HandlerFixture, "handler: missing key", "[handler]") {
  const auto key = GENERATE(as<std::string>{}, kProtocol, kAdapter);

  auto cfg = LoadJson(TKK_TEST_DATA_DIR "/valid/s7/base0.json");
  REQUIRE(cfg.erase(key) == 1);
  WriteToS7Dir("valid_cfg1", cfg);
  tkk::adapter::Handler handler{dir};
  auto res = handler.Start();
  REQUIRE_FALSE(res); //  Error
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {key});
  const auto status = handler.GetStatus();
  CHECK(status.adapter_count == 0);
  CHECK(status.active_ids.size() == 0);
}

TEST_CASE_METHOD(HandlerFixture, "handler: unknown protocol", "[handler]") {
  auto cfg = LoadJson(TKK_TEST_DATA_DIR "/valid/s7/base0.json");
  REQUIRE(cfg.erase(kProtocol) == 1);
  WriteToS7Dir("valid_cfg", cfg);
  tkk::adapter::Handler handler{dir};
  auto res = handler.Start();
  REQUIRE_FALSE(res); //  Error
  CheckError(std::move(res).error(), tkk::ErrC::kConfigInvalid, {kProtocol});
  const auto status = handler.GetStatus();
  CHECK(status.adapter_count == 0);
  CHECK(status.active_ids.size() == 0);
}