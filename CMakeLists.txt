cmake_minimum_required(VERSION 3.14)

include(cmake/ConfigureCompiler.cmake)

project(soso_sc_position_copy)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(SC_PATH "" CACHE PATH "Path to SierraChart installation")

include_directories("${SC_PATH}/ACS_Source")

add_subdirectory(src)
add_subdirectory(test)
