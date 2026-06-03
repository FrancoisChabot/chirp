add_executable(chirp main.cpp)
target_link_libraries(chirp PRIVATE chirp_parser chirp_interpreter)
target_compile_features(chirp PUBLIC cxx_std_20)
