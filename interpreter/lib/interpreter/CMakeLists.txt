add_library(chirp_interpreter src/value.cpp src/evaluator.cpp)
target_include_directories(chirp_interpreter PUBLIC include)
target_link_libraries(chirp_interpreter PUBLIC chirp_backend chirp_bigint PRIVATE chirp_frontend)
target_compile_features(chirp_interpreter PUBLIC cxx_std_20)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
