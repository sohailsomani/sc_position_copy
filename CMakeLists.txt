cmake_minimum_required(VERSION 3.14)

include(cmake/ConfigureCompiler.cmake)

project(soso_sc_position_copy)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_subdirectory(src)
add_subdirectory(test)
