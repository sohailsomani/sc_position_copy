include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
  )

FetchContent_MakeAvailable(googletest)

enable_testing()

file(GLOB_RECURSE SOURCES *.cpp)

add_executable(test_sc_position_copy ${SOURCES})

target_link_libraries(test_sc_position_copy sc_position_copy gtest_main)

include(GoogleTest)
gtest_discover_tests(test_sc_position_copy)

add_custom_command(TARGET test_sc_position_copy POST_BUILD COMMAND ctest)
