include(FetchContent)

FetchContent_Declare(
  boost
  URL https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.tar.gz
  URL_HASH SHA256=5347464af5b14ac54bb945dc68f1dd7c56f0dad7262816b956138fc53bcc0131
  SOURCE_DIR tools/cmake/include
  )

FetchContent_MakeAvailable(boost)
FetchContent_GetProperties(boost)

file(GLOB_RECURSE SERIALIZATION_SOURCES "${boost_SOURCE_DIR}/libs/serialization/src/*.cpp")
add_library(boost STATIC ${SERIALIZATION_SOURCES})

target_include_directories(boost PUBLIC ${boost_SOURCE_DIR})
target_link_libraries(boost PUBLIC ws2_32)
