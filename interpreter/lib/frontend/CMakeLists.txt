add_library(chirp_frontend src/lexer.cpp src/parser.cpp src/ast_printer.cpp src/literal_decode.cpp)
target_include_directories(chirp_frontend PUBLIC include)
target_compile_features(chirp_frontend PUBLIC cxx_std_20)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
