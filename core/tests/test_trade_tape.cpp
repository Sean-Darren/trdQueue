#include "trdqueue/trade_tape.hpp"

#include <gtest/gtest.h>

using trdqueue::Side;
using trdqueue::Trade;
using trdqueue::TradeTape;

TEST(TradeTape, PushPastCapacityDropsOldest) {
    TradeTape tape(3);
    tape.push(Trade{Side::Buy, 100, 1, 1});
    tape.push(Trade{Side::Sell, 101, 2, 2});
    tape.push(Trade{Side::Buy, 102, 3, 3});
    EXPECT_EQ(tape.size(), 3u);
    EXPECT_EQ(tape.at(0).trade_id, 1);
    EXPECT_EQ(tape.at(2).trade_id, 3);

    tape.push(Trade{Side::Sell, 103, 4, 4});
    EXPECT_EQ(tape.size(), 3u);
    EXPECT_EQ(tape.at(0).trade_id, 2);
    EXPECT_EQ(tape.at(1).trade_id, 3);
    EXPECT_EQ(tape.at(2).trade_id, 4);

    tape.push(Trade{Side::Buy, 104, 5, 5});
    EXPECT_EQ(tape.at(0).trade_id, 3);
    EXPECT_EQ(tape.at(1).trade_id, 4);
    EXPECT_EQ(tape.at(2).trade_id, 5);
}

TEST(TradeTape, EmptyCapacityIsNoOp) {
    TradeTape tape(0);
    tape.push(Trade{Side::Buy, 1, 1, 1});
    EXPECT_EQ(tape.size(), 0u);
}
