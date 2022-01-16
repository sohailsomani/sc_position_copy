file(GLOB_RECURSE SOURCES *.cpp)

add_executable(test_primary ${SOURCES})

target_link_libraries(test_primary primary gtest_main)

gtest_discover_tests(test_primary)

add_custom_command(TARGET test_primary POST_BUILD COMMAND ctest)

