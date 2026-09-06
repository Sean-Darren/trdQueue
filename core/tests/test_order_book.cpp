#include "trdqueue/decimal.hpp"
#include "trdqueue/order_book.hpp"

#include <gtest/gtest.h>

using trdqueue::LevelUpdate;
using trdqueue::OrderBook;
using trdqueue::Side;
using trdqueue::parse_decimal_to_ticks;

namespace {

std::int64_t ticks(std::string_view s) {
    const auto v = parse_decimal_to_ticks(s);
    EXPECT_TRUE(v.has_value()) << s;
    return v.value_or(0);
}

}

TEST(OrderBook, EmptyHasNoBbo) {
    const OrderBook book;
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_EQ(book.bid_count(), 0u);
    EXPECT_EQ(book.ask_count(), 0u);
}

TEST(OrderBook, SnapshotSetsBestBidAsk) {
    OrderBook book;
    book.apply_snapshot(
        {{ticks("10101.10"), ticks("0.45054140")},
         {ticks("10100.00"), ticks("1.00000000")}},
        {{ticks("10102.55"), ticks("0.57753524")},
         {ticks("10103.00"), ticks("2.00000000")}});

    const auto bid = book.best_bid();
    const auto ask = book.best_ask();
    ASSERT_TRUE(bid.has_value());
    ASSERT_TRUE(ask.has_value());
    EXPECT_EQ(bid->price_ticks, ticks("10101.10"));
    EXPECT_EQ(bid->size_ticks, ticks("0.45054140"));
    EXPECT_EQ(ask->price_ticks, ticks("10102.55"));
    EXPECT_EQ(ask->size_ticks, ticks("0.57753524"));
    EXPECT_EQ(book.bid_count(), 2u);
    EXPECT_EQ(book.ask_count(), 2u);
}

TEST(OrderBook, UpdateChangesSizeAtExistingLevel) {
    OrderBook book;
    book.apply_snapshot({{ticks("10101.10"), ticks("0.45054140")}},
                        {{ticks("10102.55"), ticks("0.57753524")}});

    book.apply_level_update(
        LevelUpdate{Side::Sell, ticks("10102.55"), ticks("1.00000000")});

    EXPECT_EQ(book.size_at(Side::Sell, ticks("10102.55")), ticks("1.00000000"));
    const auto ask = book.best_ask();
    ASSERT_TRUE(ask.has_value());
    EXPECT_EQ(ask->price_ticks, ticks("10102.55"));
    EXPECT_EQ(ask->size_ticks, ticks("1.00000000"));
}

TEST(OrderBook, SizeZeroRemovesLevelAndMovesBbo) {
    OrderBook book;
    book.apply_snapshot(
        {{ticks("10101.10"), ticks("0.45054140")},
         {ticks("10100.00"), ticks("1.00000000")}},
        {{ticks("10102.55"), ticks("0.57753524")}});

    book.apply_level_update(
        LevelUpdate{Side::Buy, ticks("10101.10"), ticks("0")});

    EXPECT_EQ(book.size_at(Side::Buy, ticks("10101.10")), 0);
    EXPECT_EQ(book.bid_count(), 1u);
    const auto bid = book.best_bid();
    ASSERT_TRUE(bid.has_value());
    EXPECT_EQ(bid->price_ticks, ticks("10100.00"));
    EXPECT_EQ(bid->size_ticks, ticks("1.00000000"));
}

TEST(OrderBook, NewLevelBecomesBestBid) {
    OrderBook book;
    book.apply_snapshot({{ticks("10101.10"), ticks("0.45054140")}},
                        {{ticks("10102.55"), ticks("0.57753524")}});

    book.apply_level_update(
        LevelUpdate{Side::Buy, ticks("10101.80000000"), ticks("0.162567")});

    const auto bid = book.best_bid();
    ASSERT_TRUE(bid.has_value());
    EXPECT_EQ(bid->price_ticks, ticks("10101.80000000"));
    EXPECT_EQ(bid->size_ticks, ticks("0.162567"));
}

TEST(OrderBook, OneSidedBook) {
    OrderBook book;
    book.apply_snapshot({{ticks("100.00"), ticks("1")}}, {});
    EXPECT_TRUE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());

    book.apply_snapshot({}, {{ticks("101.00"), ticks("2")}});
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_TRUE(book.best_ask().has_value());
}

TEST(OrderBook, SnapshotReplacesEntireBook) {
    OrderBook book;
    book.apply_snapshot({{ticks("1.00"), ticks("1")}}, {{ticks("2.00"), ticks("1")}});
    book.apply_snapshot({{ticks("3.00"), ticks("4")}}, {{ticks("5.00"), ticks("6")}});

    EXPECT_EQ(book.size_at(Side::Buy, ticks("1.00")), 0);
    const auto bid = book.best_bid();
    const auto ask = book.best_ask();
    ASSERT_TRUE(bid.has_value());
    ASSERT_TRUE(ask.has_value());
    EXPECT_EQ(bid->price_ticks, ticks("3.00"));
    EXPECT_EQ(ask->price_ticks, ticks("5.00"));
}
