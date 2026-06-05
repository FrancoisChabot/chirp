add_library(chirp_bigint src/bigint.cpp)
target_include_directories(chirp_bigint PUBLIC include)
target_compile_features(chirp_bigint PUBLIC cxx_std_20)

add_subdirectory(test)
