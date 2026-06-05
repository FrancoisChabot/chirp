cmake_minimum_required(VERSION 3.20)
project(Chirp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CHIRP_ENABLE_SOURCE_TREE_ROOT "Enable source-tree Chirp root fallback for development builds" ON)

enable_testing()
add_subdirectory(interpreter)
