cmake_minimum_required(VERSION 3.25)
project(S7EmbeddedDriver LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Snap7 library
add_library(snap7micro SHARED
    include/third_party/snap7micro/snap_msgsock.cpp
    include/third_party/snap7micro/snap_sysutils.cpp
    include/third_party/snap7micro/s7_micro_client.cpp
    include/third_party/snap7micro/s7_isotcp.cpp
    include/third_party/snap7micro/s7_peer.cpp
)
target_include_directories(snap7micro SYSTEM PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/snap7micro)
set_target_properties(snap7micro PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION 1.0.0
    SOVERSION 1
)

# Json
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/nlohmann)

# utfcpp
add_library(utfcpp INTERFACE)
target_include_directories(utfcpp SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/utfcpp)

# Sqlite
add_library(sqlite_cpp SHARED
  include/third_party/sqlitecpp/src/Backup.cpp
  include/third_party/sqlitecpp/src/Column.cpp
  include/third_party/sqlitecpp/src/Database.cpp
  include/third_party/sqlitecpp/src/Exception.cpp
  include/third_party/sqlitecpp/src/Savepoint.cpp
  include/third_party/sqlitecpp/src/Statement.cpp
  include/third_party/sqlitecpp/src/Transaction.cpp
)
target_include_directories(sqlite_cpp SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/sqlitecpp/include)
find_package(SQLite3 REQUIRED)
target_link_libraries(sqlite_cpp PUBLIC SQLite3::SQLite3)


# TKK gateway 
add_library(tkk_core STATIC
    src/adapter/s7/helper.cpp
    src/adapter/s7/config_validation.cpp
    src/adapter/s7/config_rule.cpp
    src/adapter/s7/batch_creation.cpp
    src/adapter/s7/state.cpp
    src/adapter/s7/adapter.cpp
    src/adapter/s7/error.cpp
    src/adapter/handler.cpp
    src/core/error.cpp
    src/core/util.cpp
)
target_include_directories(tkk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(tkk_core PUBLIC snap7micro nlohmann_json utfcpp sqlite_cpp)

add_executable(S7EmbeddedDriver src/main.cpp)
target_link_libraries(S7EmbeddedDriver PRIVATE tkk_core)
target_include_directories(S7EmbeddedDriver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
 
if(CMAKE_CROSSCOMPILING)
  set(TKK_CONFIG_DIR_DEFAULT "/etc/tkk-iotgateway")
else()
  set(TKK_CONFIG_DIR_DEFAULT "${CMAKE_SOURCE_DIR}/test/config")
endif()
set(TKK_CONFIG_DIR "${TKK_CONFIG_DIR_DEFAULT}" CACHE STRING "Config directory")

target_compile_definitions(S7EmbeddedDriver PRIVATE TKK_CONFIG_DIR="${TKK_CONFIG_DIR}")
target_compile_options(tkk_core PRIVATE -Wall -Wextra -Wpedantic)

include(GNUInstallDirs)
install(TARGETS S7EmbeddedDriver RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS snap7micro LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})


# Unit test build
option(TKK_BUILD_TESTS "Build unit tests" ON)
if(TKK_BUILD_TESTS)
  enable_testing()
  find_package(Catch2 3 REQUIRED)
  include(Catch)

set(TEST_SOURCES
    test/adapter/handler.cpp
    test/adapter/s7/config_validation.cpp
    test/adapter/s7/batch_creation.cpp
    test/adapter/s7/adapter.cpp
    test/adapter/s7/state.cpp
)

  add_executable(tkk_tests ${TEST_SOURCES})

target_compile_definitions(tkk_tests PRIVATE TKK_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/test/config")
target_include_directories(tkk_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/)
  target_link_libraries(tkk_tests PRIVATE tkk_core Catch2::Catch2WithMain)
  catch_discover_tests(tkk_tests)
endif()
