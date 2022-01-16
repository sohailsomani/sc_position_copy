file(GLOB_RECURSE SOURCES *.cpp)

add_executable(test_secondary ${SOURCES})

target_link_libraries(test_secondary secondary gtest_main)

gtest_discover_tests(test_secondary)

add_custom_command(TARGET test_secondary POST_BUILD COMMAND ctest)

