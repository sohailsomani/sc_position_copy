#include "soso/sc_position_copy/primary.hpp"
#include "gtest/gtest.h"

TEST(HelloPrimaryTest, BasicTest) {
  EXPECT_STREQ(hello_primary(), "world");
}
