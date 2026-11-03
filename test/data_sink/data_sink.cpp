#include "tkk/data_sink/data_sink.hpp"
#include "helper/helper.hpp"
#include <filesystem>

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
  tkk::sink::Sink sink{dir};
  auto res = sink.Start();
  REQUIRE(res);
  REQUIRE(std::filesystem::exists(dir / "sink.sqlite"));
  CHECK(std::filesystem::is_regular_file(dir / "sink.sqlite"));
}

TEST_CASE_METHOD(SinkFixture, "sink: invalid db path", "[sink]") {
  const auto fake_dir = dir / "fake_path";
  tkk::sink::Sink sink{fake_dir};
  auto res = sink.Start();
  REQUIRE_FALSE(res);
  CHECK_FALSE(std::filesystem::exists(fake_dir / "sink.sqlite"));
  CheckError(std::move(res).error(), tkk::ErrC::kFileError, {"unable", "sink", "database"});
}