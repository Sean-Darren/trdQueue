#include "trdqueue/decimal.hpp"

#include <gtest/gtest.h>

using trdqueue::parse_decimal_to_ticks;
using trdqueue::ticks_to_decimal;

TEST(Decimal, ParsesCoinbaseDocExamples) {
    EXPECT_EQ(*parse_decimal_to_ticks("10101.10"), 10101 * 100000000LL + 10000000LL);
    EXPECT_EQ(*parse_decimal_to_ticks("0.45054140"), 45054140LL);
    EXPECT_EQ(*parse_decimal_to_ticks("0"), 0);
    EXPECT_EQ(*parse_decimal_to_ticks("0.00000000"), 0);
    EXPECT_EQ(*parse_decimal_to_ticks("0.162567"), 16256700LL);
    EXPECT_EQ(*parse_decimal_to_ticks("10101.80000000"), 1010180000000LL);
}

TEST(Decimal, RejectsJunk) {
    EXPECT_FALSE(parse_decimal_to_ticks("").has_value());
    EXPECT_FALSE(parse_decimal_to_ticks("-1.0").has_value());
    EXPECT_FALSE(parse_decimal_to_ticks("1.2.3").has_value());
    EXPECT_FALSE(parse_decimal_to_ticks("abc").has_value());
    EXPECT_FALSE(parse_decimal_to_ticks("1.000000001").has_value());
}

TEST(Decimal, RoundTripStripsTrailingZeros) {
    EXPECT_EQ(ticks_to_decimal(*parse_decimal_to_ticks("10101.10")), "10101.1");
    EXPECT_EQ(ticks_to_decimal(*parse_decimal_to_ticks("0.162567")), "0.162567");
    EXPECT_EQ(ticks_to_decimal(*parse_decimal_to_ticks("100")), "100");
}
