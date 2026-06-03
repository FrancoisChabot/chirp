add_library(chirp_parser src/lexer.cpp src/parser.cpp src/ast_printer.cpp)
target_include_directories(chirp_parser PUBLIC include)
target_compile_features(chirp_parser PUBLIC cxx_std_20)

add_subdirectory(test)
