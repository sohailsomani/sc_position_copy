file(GLOB_RECURSE SOURCES *.cpp)

add_library(secondary STATIC ${SOURCES})

target_include_directories(secondary PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
