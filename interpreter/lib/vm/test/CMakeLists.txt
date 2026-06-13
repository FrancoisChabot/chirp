add_executable(chirp_vm_test test_vm.cpp)
target_link_libraries(chirp_vm_test PRIVATE chirp_vm gtest_main)
target_compile_features(chirp_vm_test PUBLIC cxx_std_20)

include(GoogleTest)
gtest_discover_tests(chirp_vm_test)
