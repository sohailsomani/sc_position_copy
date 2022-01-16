file(GLOB_RECURSE SOURCES *.cpp)

add_executable(test_sc_position_copy_secondary ${SOURCES})

target_link_libraries(test_sc_position_copy_secondary sc_position_copy_secondary gtest_main)

gtest_discover_tests(test_sc_position_copy_secondary)

add_custom_command(TARGET test_sc_position_copy_secondary POST_BUILD COMMAND ctest)

