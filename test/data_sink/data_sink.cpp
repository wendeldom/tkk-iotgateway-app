#include "helper/helper.hpp"
#include "tkk/core/type.hpp"
#include "tkk/data_sink/sink_engine.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <vector>

namespace {

// Fixture for testing the Sink class
struct SinkFixture {
  std::filesystem::path dir;

  SinkFixture() {
    dir = std::filesystem::temp_directory_path() / "tkk_test_sink";
    std::filesystem::create_directories(dir);
  }

  ~SinkFixture() { RemoveTemp(); }

  void RemoveTemp() const {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  }
};
} // namespace

TEST_CASE_METHOD(SinkFixture, "sink: valid db path", "[sink]") {
  tkk::sink::SinkEngine e1{dir};
  auto res = e1.OpenDatabase();
  REQUIRE(res);
  REQUIRE(std::filesystem::exists(dir / "sink.sqlite"));
  CHECK(std::filesystem::is_regular_file(dir / "sink.sqlite"));
}

TEST_CASE_METHOD(SinkFixture, "sink: invalid db path", "[sink]") {
  const auto fake_dir = dir / "fake_path";
  tkk::sink::SinkEngine e1{fake_dir};
  auto res = e1.OpenDatabase();
  REQUIRE_FALSE(res);
  CHECK_FALSE(std::filesystem::exists(fake_dir / "sink.sqlite"));
  CheckError(std::move(res).error(), tkk::ErrC::kFileError, {"unable", "sink", "database"});
}

TEST_CASE_METHOD(SinkFixture, "sink: insert 5 data points", "[sink]") {
  tkk::sink::SinkEngine e1{dir};
  auto res = e1.OpenDatabase();
  REQUIRE(res);

  std::vector<tkk::DataPoint> data_points;
  data_points.reserve(5);
  for (int i = 0; i < 5; ++i) {
    data_points.push_back(tkk::DataPoint{
        .id = static_cast<uint32_t>(i),
        .quality = tkk::Quality::kGood,
        .data = std::vector<tkk::U8>{0, 1, 2, 3, 4},
        .type = "type" + std::to_string(i),
        .timestamp = std::chrono::system_clock::now(),
        .name = "name" + std::to_string(i),
    });
  }
  e1.Push(std::move(data_points));
  CHECK(e1.DataAvailable());
  CHECK(e1.GetQueueSize() == 1);
  auto run_res = e1.RunOnce();
  REQUIRE(run_res);
  CHECK_FALSE(e1.DataAvailable());
  CHECK(e1.GetQueueSize() == 0);
  auto table_size_res = e1.GetTableSize();
  REQUIRE(table_size_res);
  CHECK(*table_size_res == 5);
}