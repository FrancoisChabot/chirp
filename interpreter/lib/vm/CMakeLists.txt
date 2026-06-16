add_library(chirp_vm
  src/nature.cpp
  src/Value.cpp
  src/vm.cpp
  src/intrinsics.cpp
)

target_include_directories(chirp_vm PUBLIC include PRIVATE src)
target_compile_features(chirp_vm PUBLIC cxx_std_20)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
