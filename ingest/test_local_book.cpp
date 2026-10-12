#include "local_book.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

TEST(LocalBook, SnapshotJsonOmitsResultUntilAnOrder) {
    trdqueue::LocalSnapshot snapshot;
    snapshot.bids.push_back(trdqueue::LocalLevel{2, "100", "1.5"});
    snapshot.asks.push_back(trdqueue::LocalLevel{3, "101", "2"});
    snapshot.trades.push_back(trdqueue::TradeView{"sell", "100", "1", 1});

    const nlohmann::json parsed = nlohmann::json::parse(trdqueue::local_snapshot_json(snapshot));
    EXPECT_EQ(parsed["channel"], "local");
    EXPECT_FALSE(parsed.contains("message"));
    EXPECT_EQ(parsed["bids"][0]["id"], 2);
    EXPECT_EQ(parsed["bids"][0]["price"], "100");
    EXPECT_EQ(parsed["asks"][0]["size"], "2");
    EXPECT_EQ(parsed["trades"][0]["trade_id"], 1);
}

TEST(LocalBook, ResultJsonIncludesFill) {
    trdqueue::LocalSnapshot snapshot;
    snapshot.include_result = true;
    snapshot.accepted = true;
    snapshot.order_id = 4;
    snapshot.resting_size = "0";
    snapshot.message = "filled";
    snapshot.fills.push_back(trdqueue::TradeView{"sell", "100", "1", 7});

    const nlohmann::json parsed = nlohmann::json::parse(trdqueue::local_snapshot_json(snapshot));
    EXPECT_TRUE(parsed["accepted"].get<bool>());
    EXPECT_EQ(parsed["order_id"], 4);
    EXPECT_EQ(parsed["message"], "filled");
    EXPECT_EQ(parsed["fills"][0]["price"], "100");
}
