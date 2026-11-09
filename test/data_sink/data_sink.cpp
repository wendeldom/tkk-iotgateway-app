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

auto FillDataPoints(int count) -> std::vector<tkk::DataPoint> {
  std::vector<tkk::DataPoint> data_points;
  data_points.reserve(count);
  for (int i = 0; i < count; ++i) {
    data_points.push_back(tkk::DataPoint{
        .id = static_cast<uint32_t>(i),
        .quality = tkk::Quality::kGood,
        .data = std::vector<tkk::U8>{0, 1, 2, 3, 4},
        .type = "type" + std::to_string(i),
        .timestamp = std::chrono::system_clock::now(),
        .name = "name" + std::to_string(i),
    });
  }
  return data_points;
}

auto FillDataPointsWithNullData(int count) -> std::vector<tkk::DataPoint> {
  std::vector<tkk::DataPoint> data_points;
  data_points.reserve(count);
  for (int i = 0; i < count; ++i) {
    data_points.push_back(tkk::DataPoint{
        .id = static_cast<uint32_t>(i),
        .quality = tkk::Quality::kGood,
        .data = std::vector<tkk::U8>{},
        .type = "type" + std::to_string(i),
        .timestamp = std::chrono::system_clock::now(),
        .name = "name" + std::to_string(i),
    });
  }
  return data_points;
}
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

TEST_CASE_METHOD(SinkFixture, "sink: insert 5 data points (same batch)", "[sink]") {
  tkk::sink::SinkEngine e1{dir};
  auto res = e1.OpenDatabase();
  REQUIRE(res);

  auto data_points = FillDataPoints(5);
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

TEST_CASE_METHOD(SinkFixture, "sink: insert 10 data points (different batches)", "[sink]") {
  tkk::sink::SinkEngine e1{dir};
  auto res = e1.OpenDatabase();
  REQUIRE(res);

  auto data_points_1 = FillDataPoints(5);
  e1.Push(std::move(data_points_1));
  auto data_points_2 = FillDataPoints(5);
  e1.Push(std::move(data_points_2));
  CHECK(e1.DataAvailable());
  CHECK(e1.GetQueueSize() == 2);
  auto run_res = e1.RunOnce();
  REQUIRE(run_res);
  CHECK_FALSE(e1.DataAvailable());
  CHECK(e1.GetQueueSize() == 0);
  auto table_size_res = e1.GetTableSize();
  REQUIRE(table_size_res);
  CHECK(*table_size_res == 10);
}

TEST_CASE_METHOD(SinkFixture, "sink: insert NULL value into db", "[sink]") {
  tkk::sink::SinkEngine e1{dir};
  auto res = e1.OpenDatabase();
  REQUIRE(res);

  auto data_points = FillDataPointsWithNullData(5);
  e1.Push(std::move(data_points));
  CHECK(e1.DataAvailable());
  CHECK(e1.GetQueueSize() == 1);
  auto run_res = e1.RunOnce();
  // sqlite should throw an error when trying to insert NULL data
  REQUIRE_FALSE(run_res);
  CheckError(std::move(run_res).error(), tkk::ErrC::kDataBaseError, {"unable", "entry", "write"});
  CHECK(e1.DataAvailable());
  CHECK(e1.GetQueueSize() == 1);
  auto table_size_res = e1.GetTableSize();
  REQUIRE(table_size_res);
  CHECK(*table_size_res == 0);
}

TEST_CASE_METHOD(SinkFixture, "sink: remove 3 data points", "[sink]") {
  // tkk::sink::SinkEngine e1{dir};
  // auto res = e1.OpenDatabase();
  // REQUIRE(res);

  // auto data_points_1 = FillDataPoints(5);
  // e1.Push(std::move(data_points_1));
  // auto data_points_2 = FillDataPoints(5);
  // e1.Push(std::move(data_points_2));
  // CHECK(e1.DataAvailable());
  // CHECK(e1.GetQueueSize() == 2);
  // auto run_res = e1.RunOnce();
  // REQUIRE(run_res);
  // CHECK_FALSE(e1.DataAvailable());
  // CHECK(e1.GetQueueSize() == 0);
  // auto table_size_res = e1.GetTableSize();
  // REQUIRE(table_size_res);
  // CHECK(*table_size_res == 10);
}