#include "coinbase_parse.hpp"
#include "trdqueue/decimal.hpp"
#include "trdqueue/order_book.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>

TEST(CoinbaseParse, FixtureMatchesDemoBbo) {
    std::ifstream in(TRDQUEUE_FIXTURE_PATH);
    ASSERT_TRUE(in.is_open());

    nlohmann::json root;
    in >> root;

    trdqueue::OrderBook book;
    bool saw_snapshot = false;
    for (const auto& msg : root.at("messages")) {
        const auto parsed = trdqueue::parse_exchange_message(msg);
        ASSERT_TRUE(parsed.ok) << parsed.error;
        if (parsed.message.type == trdqueue::ExchangeMsgType::Snapshot) {
            book.apply_snapshot(parsed.message.bids, parsed.message.asks);
            saw_snapshot = true;
        } else if (parsed.message.type == trdqueue::ExchangeMsgType::L2Update) {
            ASSERT_TRUE(saw_snapshot);
            for (const auto& update : parsed.message.changes) {
                book.apply_level_update(update);
            }
        } else {
            FAIL() << parsed.message.type_name;
        }
    }

    const auto bid = book.best_bid();
    const auto ask = book.best_ask();
    ASSERT_TRUE(bid.has_value());
    ASSERT_TRUE(ask.has_value());
    EXPECT_EQ(trdqueue::ticks_to_decimal(bid->price_ticks), "10101.8");
    EXPECT_EQ(trdqueue::ticks_to_decimal(bid->size_ticks), "0.162567");
    EXPECT_EQ(trdqueue::ticks_to_decimal(ask->price_ticks), "10102.55");
    EXPECT_EQ(trdqueue::ticks_to_decimal(ask->size_ticks), "1");
}

TEST(CoinbaseParse, RejectsBadPrice) {
    const auto parsed = trdqueue::parse_exchange_message(nlohmann::json::parse(
        R"({"type":"l2update","product_id":"BTC-USD","changes":[["buy","nope","1"]]})"));
    EXPECT_FALSE(parsed.ok);
}

TEST(CoinbaseParse, HeartbeatIsNotABookUpdate) {
    const auto parsed = trdqueue::parse_exchange_message(nlohmann::json::parse(
        R"({"type":"heartbeat","product_id":"BTC-USD"})"));
    ASSERT_TRUE(parsed.ok);
    EXPECT_EQ(parsed.message.type, trdqueue::ExchangeMsgType::Heartbeat);
}
