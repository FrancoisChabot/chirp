cmake_minimum_required(VERSION 3.20)
project(ChirpInterpreter VERSION 0.1.0 LANGUAGES CXX)

enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(lib/parser)
add_subdirectory(lib/interpreter)
add_subdirectory(app)
