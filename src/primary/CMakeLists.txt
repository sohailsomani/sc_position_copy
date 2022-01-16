file(GLOB_RECURSE SOURCES *.cpp)

add_library(primary STATIC ${SOURCES})

target_include_directories(primary PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
