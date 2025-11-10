cmake_minimum_required(VERSION 3.21)
project(
  cpp-coro
  VERSION 0.1.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SRCS src/*.cpp)

add_executable(cpp-coro ${SRCS})

cmake_policy(SET CMP0167 OLD)
find_package(OpenSSL REQUIRED)
find_package(Boost 1.88 REQUIRED CONFIG COMPONENTS system)

target_compile_options(
  cpp-coro
  PRIVATE -march=native
          -Wall
          -Wextra
          -Werror
          -Wattributes
          -Wconversion
          -Wduplicated-cond
          -Wduplicated-branches
          -Wformat
          -Wimplicit-fallthrough
          -Wpedantic
          -fcoroutines
          # false positives in coroutine frame allocations
          -Wno-mismatched-new-delete
          # something from boost is triggering this
          -Wno-array-bounds
          -Wno-stringop-overflow)

target_include_directories(cpp-coro PRIVATE src/)
target_compile_definitions(
  cpp-coro PRIVATE # BOOST_ASIO_HAS_IO_URING_AS_DEFAULT=1
                   # BOOST_ASIO_ENABLE_HANDLER_TRACKING=1
)
target_link_libraries(cpp-coro PRIVATE Boost::boost OpenSSL::SSL
                                       OpenSSL::Crypto)

if(DEFINED ENV{TSAN})
  message(WARNING "enabling tsan")
  target_compile_options(cpp-coro PRIVATE -fsanitize=thread
                                          -fno-omit-frame-pointer -Wno-tsan)
  target_link_options(cpp-coro PRIVATE -fsanitize=thread -static-libtsan)
endif()
