file(GLOB_RECURSE SOURCES *.cpp)

add_library(sc_position_copy_secondary STATIC ${SOURCES})

target_include_directories(sc_position_copy_secondary PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include/)
