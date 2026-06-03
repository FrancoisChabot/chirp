add_library(chirp_interpreter src/value.cpp)
target_include_directories(chirp_interpreter PUBLIC include)
target_compile_features(chirp_interpreter PUBLIC cxx_std_20)

add_subdirectory(test)
