cmake_minimum_required(VERSION 3.22)

if (EXISTS "${CMAKE_SOURCE_DIR}/build/conan_toolchain.cmake" AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/build/conan_toolchain.cmake" CACHE FILEPATH "" FORCE)
endif()

project(Astro
  VERSION 0.1.0
  DESCRIPTION "Astro - a minimal C++20 blockchain node (MVP)"
  LANGUAGES CXX
)

option(ASTRO_BUILD_TESTS "Build unit tests" ON)
option(ASTRO_ENABLE_SANITIZERS "Enable Address/Undefined sanitizers in Debug" ON)
option(ASTRO_WITH_ROCKSDB "Enable RocksDB-backed store (optional)" OFF)
option(ASTRO_WITH_NET "Enable net/p2p stubs (Boost.Asio if available)" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ASTRO_ENABLE_SANITIZERS AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")

find_package(spdlog QUIET CONFIG)
if(spdlog_FOUND)
  message(STATUS "spdlog found: ${spdlog_DIR}")
else()
  message(STATUS "spdlog not found - will use simple printf fallback")
endif()

find_package(sodium QUIET)  # libsodium
if(sodium_FOUND)
  add_compile_definitions(ASTRO_HAVE_SODIUM)
  message(STATUS "libsodium found")
endif()
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
  message(STATUS "OpenSSL found")
endif()

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()
if(ASTRO_WITH_NET)
  find_package(Boost QUIET COMPONENTS system)
  if(Boost_FOUND)
    add_compile_definitions(ASTRO_HAVE_BOOST)
    message(STATUS "Boost found for net/p2p")
  endif()
endif()

if(ASTRO_WITH_ROCKSDB)
  find_package(rocksdb QUIET CONFIG)
  if(rocksdb_FOUND)
    add_compile_definitions(ASTRO_HAVE_ROCKSDB)
    message(STATUS "RocksDB found")
  else()
    message(STATUS "RocksDB not found - will use file-backed store")
  endif()
endif()

set(ASTRO_CORE_SOURCES "")
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/core/chain.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/core/chain.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/store/blockstore.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/store/blockstore.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/core/hash.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/core/hash.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/core/keys.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/core/keys.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/core/transaction.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/core/transaction.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/core/block.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/core/block.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/core/merkle.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/core/merkle.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/core/pow.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/core/pow.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/core/miner.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/core/miner.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/net/p2p.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/net/p2p.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/server.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/rpc/server.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/wallet/wallet.cpp)
  list(APPEND ASTRO_CORE_SOURCES src/wallet/wallet.cpp)
endif()

if(ASTRO_CORE_SOURCES)
  add_library(astro_core ${ASTRO_CORE_SOURCES})
  target_include_directories(astro_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  if(spdlog_FOUND)
    target_link_libraries(astro_core PUBLIC spdlog::spdlog_header_only)
  endif()
  if(ASTRO_WITH_ROCKSDB AND rocksdb_FOUND)
    target_link_libraries(astro_core PRIVATE rocksdb::rocksdb)
  endif()
  if(ASTRO_WITH_NET AND Boost_FOUND)
    if(TARGET Boost::system)
      target_link_libraries(astro_core PRIVATE Boost::system)
    else()
      message(STATUS "Boost::system target not available; skipping Boost link")
    endif()
  endif()
  if(OpenSSL_FOUND)
    if(TARGET OpenSSL::Crypto)
      target_link_libraries(astro_core PRIVATE OpenSSL::Crypto)
    else()
      target_link_libraries(astro_core PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
  endif()
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/keys.cpp)
  add_library(astro_crypto src/crypto/keys.cpp)
  target_include_directories(astro_crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
  if(sodium_FOUND)
    target_link_libraries(astro_crypto PUBLIC sodium)
  else()
    target_compile_definitions(astro_crypto PRIVATE ASTRO_CRYPTO_STUBS)
  endif()
endif()

set(ASTRO_NODE_SOURCES src/main.cpp)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/cli.cpp)
  list(APPEND ASTRO_NODE_SOURCES src/cli/cli.cpp)
endif()

add_executable(astro-node ${ASTRO_NODE_SOURCES})
target_include_directories(astro-node PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
if(TARGET astro_core)
  target_link_libraries(astro-node PRIVATE astro_core)
endif()
if(spdlog_FOUND)
  target_link_libraries(astro-node PRIVATE spdlog::spdlog_header_only)
endif()

set_target_properties(astro-node PROPERTIES OUTPUT_NAME "astro-node")
install(TARGETS astro-node RUNTIME DESTINATION bin)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/tui.cpp)
  add_executable(astro-tui src/cli/tui.cpp)
  target_include_directories(astro-tui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  if(TARGET astro_core)
    target_link_libraries(astro-tui PRIVATE astro_core)
  endif()
  if(OpenSSL_FOUND)
    if(TARGET OpenSSL::Crypto)
      target_link_libraries(astro-tui PRIVATE OpenSSL::Crypto)
    else()
      target_link_libraries(astro-tui PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
  endif()
  set_target_properties(astro-tui PROPERTIES OUTPUT_NAME "astro-tui")
  install(TARGETS astro-tui RUNTIME DESTINATION bin)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/mine.cpp)
  add_executable(astro-mine src/cli/mine.cpp)
  target_include_directories(astro-mine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  if(TARGET astro_core)
    target_link_libraries(astro-mine PRIVATE astro_core)
  endif()
  if(OpenSSL_FOUND)
    if(TARGET OpenSSL::Crypto)
      target_link_libraries(astro-mine PRIVATE OpenSSL::Crypto)
    else()
      target_link_libraries(astro-mine PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
  endif()
  set_target_properties(astro-mine PROPERTIES OUTPUT_NAME "astro-mine")
  install(TARGETS astro-mine RUNTIME DESTINATION bin)
endif()

if(ASTRO_BUILD_TESTS)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    find_package(GTest QUIET CONFIG)
    if(NOT GTest_FOUND)
      message(STATUS "GTest not found - fetching with FetchContent (git)")
      if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
      endif()
      FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
      )
      set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
      FetchContent_MakeAvailable(googletest)
    endif()
    add_subdirectory(tests)
  elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/tests)
    enable_testing()
    find_package(GTest QUIET CONFIG)
    if(NOT GTest_FOUND)
      message(STATUS "GTest not found - fetching with FetchContent (git)")
      if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
      endif()
      FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
      )
      set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
      FetchContent_MakeAvailable(googletest)
    endif()
    add_subdirectory(src/tests)
  else()
    message(STATUS "Tests skipped (no tests/ or src/tests/ directory)")
  endif()
endif()

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
  file(GLOB_RECURSE ASTRO_ALL_CXX
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.hpp"
  )
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${ASTRO_ALL_CXX}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-format"
  )
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  add_custom_target(tidy
    COMMAND ${CLANG_TIDY_EXE} --version
    COMMENT "clang-tidy configured; analysis runs during build if enabled"
  )
endif()

add_custom_target(compdb ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
          ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
  COMMENT "Copy compile_commands.json to project root"
)