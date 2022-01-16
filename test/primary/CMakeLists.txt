file(GLOB_RECURSE SOURCES *.cpp)

add_executable(test_sc_position_copy_primary ${SOURCES})

target_link_libraries(test_sc_position_copy_primary sc_position_copy_primary gtest_main)

gtest_discover_tests(test_sc_position_copy_primary)

add_custom_command(TARGET test_sc_position_copy_primary POST_BUILD COMMAND ctest)

