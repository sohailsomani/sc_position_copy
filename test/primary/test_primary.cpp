#include "primary.hpp"
#include "gtest/gtest.h"

TEST(HelloPrimaryTest, BasicTest) { EXPECT_STREQ(hello_primary(), "world"); }
