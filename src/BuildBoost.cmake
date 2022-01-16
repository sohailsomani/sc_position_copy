include(FetchContent)

FetchContent_Declare(
  boost
  URL https://boostorg.jfrog.io/artifactory/main/release/1.78.0/source/boost_1_78_0.tar.gz
  URL_HASH SHA256=94ced8b72956591c4775ae2207a9763d3600b30d9d7446562c552f0a14a63be7
  SOURCE_DIR tools/cmake/include
  )

FetchContent_MakeAvailable(boost)
FetchContent_GetProperties(boost)

file(GLOB
  BOOST_SOURCES
  "${boost_SOURCE_DIR}/libs/serialization/src/*.cpp"
  "${boost_SOURCE_DIR}/libs/log/src/*.cpp"
  "${boost_SOURCE_DIR}/libs/log/src/windows/*.cpp"
  "${boost_SOURCE_DIR}/libs/thread/src/*.cpp"
  "${boost_SOURCE_DIR}/libs/thread/src/win32/*.cpp"
  "${boost_SOURCE_DIR}/libs/system/src/*.cpp"
  "${boost_SOURCE_DIR}/libs/json/src/*.cpp"
  )
add_library(boost STATIC ${BOOST_SOURCES})

target_include_directories(boost PUBLIC ${boost_SOURCE_DIR}
  PRIVATE "${boost_SOURCE_DIR}/libs/log/src")
target_compile_definitions(boost PRIVATE BOOST_LOG_WITHOUT_EVENT_LOG SECURITY_WIN32)
target_link_libraries(boost PUBLIC ws2_32 mswsock)
