#include "trdqueue/decimal.hpp"
#include "trdqueue/matching_engine.hpp"

#include <gtest/gtest.h>

using trdqueue::Execution;
using trdqueue::MatchingEngine;
using trdqueue::Side;
using trdqueue::parse_decimal_to_ticks;

namespace {

std::int64_t ticks(std::string_view text) {
    const auto parsed = parse_decimal_to_ticks(text);
    EXPECT_TRUE(parsed.has_value()) << text;
    return parsed.value_or(0);
}

}  // namespace

TEST(MatchingEngine, RestsWhenPricesDoNotCross) {
    MatchingEngine engine;
    const Execution bid = engine.submit(Side::Buy, ticks("100"), ticks("1"));
    const Execution ask = engine.submit(Side::Sell, ticks("101"), ticks("2"));

    EXPECT_TRUE(bid.accepted);
    EXPECT_TRUE(ask.accepted);
    EXPECT_TRUE(bid.fills.empty());
    EXPECT_TRUE(ask.fills.empty());
    ASSERT_EQ(engine.bids().size(), 1u);
    ASSERT_EQ(engine.asks().size(), 1u);
    EXPECT_EQ(engine.best_bid()->price_ticks, ticks("100"));
    EXPECT_EQ(engine.best_bid()->size_ticks, ticks("1"));
    EXPECT_EQ(engine.best_ask()->price_ticks, ticks("101"));
    EXPECT_EQ(engine.best_ask()->size_ticks, ticks("2"));
}

TEST(MatchingEngine, BuyLiftsBestAskAtMakerPrice) {
    MatchingEngine engine;
    engine.submit(Side::Sell, ticks("101"), ticks("1"));
    engine.submit(Side::Sell, ticks("100"), ticks("1"));

    const Execution buy = engine.submit(Side::Buy, ticks("101"), ticks("1"));
    ASSERT_EQ(buy.fills.size(), 1u);
    EXPECT_EQ(buy.fills[0].price_ticks, ticks("100"));
    EXPECT_EQ(buy.fills[0].size_ticks, ticks("1"));
    EXPECT_EQ(buy.fills[0].side, Side::Sell);
    EXPECT_EQ(buy.resting_size, 0);
    ASSERT_EQ(engine.asks().size(), 1u);
    EXPECT_EQ(engine.asks()[0].price_ticks, ticks("101"));
}

TEST(MatchingEngine, SamePriceFillsEarlierOrderFirst) {
    MatchingEngine engine;
    engine.submit(Side::Sell, ticks("100"), ticks("1"));
    engine.submit(Side::Sell, ticks("100"), ticks("2"));

    const Execution buy = engine.submit(Side::Buy, ticks("100"), ticks("1.5"));
    ASSERT_EQ(buy.fills.size(), 2u);
    EXPECT_EQ(buy.fills[0].size_ticks, ticks("1"));
    EXPECT_EQ(buy.fills[1].size_ticks, ticks("0.5"));
    ASSERT_EQ(engine.asks().size(), 1u);
    EXPECT_EQ(engine.asks()[0].size_ticks, ticks("1.5"));
}

TEST(MatchingEngine, SellHitsHighestBidFirst) {
    MatchingEngine engine;
    engine.submit(Side::Buy, ticks("99"), ticks("1"));
    engine.submit(Side::Buy, ticks("100"), ticks("1"));

    const Execution sell = engine.submit(Side::Sell, ticks("99"), ticks("1"));
    ASSERT_EQ(sell.fills.size(), 1u);
    EXPECT_EQ(sell.fills[0].price_ticks, ticks("100"));
    EXPECT_EQ(sell.fills[0].side, Side::Buy);
    ASSERT_EQ(engine.bids().size(), 1u);
    EXPECT_EQ(engine.bids()[0].price_ticks, ticks("99"));
}

TEST(MatchingEngine, SweepLeavesNoRestWhenFullyFilled) {
    MatchingEngine engine;
    engine.submit(Side::Sell, ticks("100"), ticks("1"));
    engine.submit(Side::Sell, ticks("101"), ticks("1"));

    const Execution buy = engine.submit(Side::Buy, ticks("101"), ticks("2"));
    EXPECT_EQ(buy.fills.size(), 2u);
    EXPECT_EQ(buy.resting_size, 0);
    EXPECT_TRUE(engine.asks().empty());
    EXPECT_FALSE(engine.best_ask().has_value());
}

TEST(MatchingEngine, PartialFillRestsTheRemainder) {
    MatchingEngine engine;
    engine.submit(Side::Sell, ticks("100"), ticks("1"));

    const Execution buy = engine.submit(Side::Buy, ticks("100"), ticks("2.5"));
    ASSERT_EQ(buy.fills.size(), 1u);
    EXPECT_EQ(buy.fills[0].size_ticks, ticks("1"));
    EXPECT_EQ(buy.resting_size, ticks("1.5"));
    ASSERT_EQ(engine.bids().size(), 1u);
    EXPECT_EQ(engine.bids()[0].id, buy.order_id);
    EXPECT_EQ(engine.bids()[0].size_ticks, ticks("1.5"));
}

TEST(MatchingEngine, CancelRemovesRestingOrder) {
    MatchingEngine engine;
    const Execution first = engine.submit(Side::Sell, ticks("100"), ticks("1"));
    const Execution second = engine.submit(Side::Sell, ticks("100"), ticks("1"));

    EXPECT_TRUE(engine.cancel(second.order_id));
    EXPECT_FALSE(engine.cancel(second.order_id));

    const Execution buy = engine.submit(Side::Buy, ticks("100"), ticks("1"));
    ASSERT_EQ(buy.fills.size(), 1u);
    EXPECT_EQ(buy.fills[0].size_ticks, ticks("1"));
    EXPECT_TRUE(engine.asks().empty());
    EXPECT_EQ(engine.asks().size(), 0u);
    EXPECT_NE(first.order_id, second.order_id);
}

TEST(MatchingEngine, RejectsNonPositivePriceOrSize) {
    MatchingEngine engine;
    EXPECT_FALSE(engine.submit(Side::Buy, ticks("100"), 0).accepted);
    EXPECT_FALSE(engine.submit(Side::Buy, 0, ticks("1")).accepted);
    EXPECT_TRUE(engine.bids().empty());
}
