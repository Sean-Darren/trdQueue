#include "book_view.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

TEST(BookView, JsonSnapshot) {
    trdqueue::BookView view;
    view.product_id = "BTC-USD";
    view.book_live = true;
    view.bid.present = true;
    view.bid.price = "10101.8";
    view.bid.size = "0.162567";
    view.ask.present = true;
    view.ask.price = "10102.55";
    view.ask.size = "1";
    view.trades.push_back(trdqueue::TradeView{"sell", "400.23", "5.23512", 10});

    const nlohmann::json parsed = nlohmann::json::parse(trdqueue::book_view_json(view));
    EXPECT_EQ(parsed["product_id"], "BTC-USD");
    EXPECT_TRUE(parsed["book_live"].get<bool>());
    EXPECT_EQ(parsed["bid"]["price"], "10101.8");
    EXPECT_EQ(parsed["bid"]["size"], "0.162567");
    EXPECT_EQ(parsed["ask"]["price"], "10102.55");
    EXPECT_EQ(parsed["ask"]["size"], "1");
    ASSERT_EQ(parsed["trades"].size(), 1u);
    EXPECT_EQ(parsed["trades"][0]["side"], "sell");
    EXPECT_EQ(parsed["trades"][0]["price"], "400.23");
    EXPECT_EQ(parsed["trades"][0]["size"], "5.23512");
    EXPECT_EQ(parsed["trades"][0]["trade_id"], 10);
}

TEST(BookView, MissingQuoteIsNull) {
    const nlohmann::json parsed = nlohmann::json::parse(trdqueue::book_view_json(trdqueue::BookView{}));
    EXPECT_FALSE(parsed["book_live"].get<bool>());
    EXPECT_TRUE(parsed["bid"].is_null());
    EXPECT_TRUE(parsed["ask"].is_null());
    EXPECT_TRUE(parsed["trades"].empty());
}
