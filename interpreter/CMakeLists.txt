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

add_subdirectory(lib/frontend)
add_subdirectory(lib/interpreter)

add_executable(chirp app/main.cpp)
target_link_libraries(chirp PRIVATE chirp_frontend chirp_interpreter)
target_compile_features(chirp PUBLIC cxx_std_20)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME chirp_e2e_tests COMMAND Python3::Interpreter "${ChirpInterpreter_SOURCE_DIR}/../scripts/test_conformance.py" $<TARGET_FILE:chirp>)

