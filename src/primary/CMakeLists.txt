file(GLOB_RECURSE SOURCES *.cpp)

add_library(primary SHARED ${SOURCES})
add_library(primary_static STATIC ${SOURCES})

target_include_directories(primary PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(primary_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(primary PRIVATE boost)
target_link_libraries(primary_static PRIVATE boost)

set_target_properties(primary PROPERTIES
  PREFIX ""
  OUTPUT_NAME "PositionCopyPrimary_64"
  )
