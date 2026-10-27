#pragma once
#include "tkk/core/error.hpp"
#include "tkk/core/type.hpp"
#include <catch2/catch_case_sensitive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <fstream>
#include <string>
#include <string_view>

using Catch::Generators::as;

inline auto LoadJson(const std::string &name) -> tkk::Json {
  std::ifstream file(name);
  REQUIRE(file.is_open());
  auto cfg = tkk::Json::parse(file);
  REQUIRE(!cfg.is_discarded());
  return cfg;
}

inline void CheckError(tkk::Error &&err, const std::string_view &substring) {
  using Catch::Matchers::ContainsSubstring;
  auto error = std::move(err).Dump();
  CHECK_THAT(error.front().Format(),
             ContainsSubstring(std::string{substring}, Catch::CaseSensitive::No));
}

inline void CheckError(tkk::Error &&err, const tkk::ErrC code) {
  using Catch::Matchers::ContainsSubstring;
  auto error = std::move(err).Dump();
  CHECK(error.front().GetCode() == code);
}

inline void CheckError(tkk::Error &&err, const tkk::ErrC code,
                       const std::vector<std::string_view> &substrings) {
  using Catch::Matchers::ContainsSubstring;
  auto error = std::move(err).Dump();
  CHECK(error.front().GetCode() == code);
  for (const auto &sub : substrings) {
    CHECK_THAT(error.front().Format(),
               ContainsSubstring(std::string{sub}, Catch::CaseSensitive::No));
  }
}