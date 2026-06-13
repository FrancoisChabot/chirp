add_library(chirp_vm src/vm.cpp src/compiler.cpp src/evaluator.cpp)
target_include_directories(chirp_vm PUBLIC include)
target_link_libraries(chirp_vm PUBLIC chirp_backend PRIVATE chirp_frontend)
target_compile_features(chirp_vm PUBLIC cxx_std_20)
