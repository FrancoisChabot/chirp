cmake_minimum_required(VERSION 3.20)
project(ChirpInterpreter VERSION 0.1.0 LANGUAGES CXX)

enable_testing()
include(GNUInstallDirs)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(lib/bigint)
add_subdirectory(lib/frontend)
add_subdirectory(lib/interpreter)

add_executable(chirp app/main.cpp)
target_link_libraries(chirp PRIVATE chirp_frontend chirp_interpreter)
target_compile_features(chirp PUBLIC cxx_std_20)

get_filename_component(CHIRP_REPO_ROOT "${ChirpInterpreter_SOURCE_DIR}/.." ABSOLUTE)

if(CHIRP_ENABLE_SOURCE_TREE_ROOT)
  target_compile_definitions(chirp PRIVATE CHIRP_ENABLE_SOURCE_TREE_ROOT=1)
else()
  target_compile_definitions(chirp PRIVATE CHIRP_ENABLE_SOURCE_TREE_ROOT=0)
endif()

target_compile_definitions(chirp PRIVATE
  CHIRP_SOURCE_ROOT_DIR="${CHIRP_REPO_ROOT}/lib/chirp"
  CHIRP_INSTALL_ROOT_DIR="${CMAKE_INSTALL_FULL_DATADIR}/chirp"
)

install(TARGETS chirp RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${CHIRP_REPO_ROOT}/lib/chirp/boot"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/chirp"
  FILES_MATCHING PATTERN "*.chirp"
)
install(DIRECTORY "${CHIRP_REPO_ROOT}/lib/chirp/std"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/chirp"
  FILES_MATCHING PATTERN "*.chirp"
)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(
  NAME chirp_e2e_tests
  COMMAND Python3::Interpreter
    "${ChirpInterpreter_SOURCE_DIR}/../scripts/test_conformance.py"
    -j auto
    --suite "${ChirpInterpreter_SOURCE_DIR}/../tests/core"
    $<TARGET_FILE:chirp>
)
