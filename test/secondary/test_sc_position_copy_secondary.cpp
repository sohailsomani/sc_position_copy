#include "soso/sc_position_copy/secondary.hpp"
#include "gtest/gtest.h"

TEST(HelloSecondaryTest, BasicTest) {
  EXPECT_STREQ(hello_secondary(), "world");
}
