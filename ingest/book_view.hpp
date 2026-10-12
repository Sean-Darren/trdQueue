#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace trdqueue {

struct QuoteView {
    bool present = false;
    std::string price;
    std::string size;
};

struct TradeView {
    std::string side;
    std::string price;
    std::string size;
    std::int64_t trade_id = 0;
};

struct BookView {
    std::string product_id;
    bool book_live = false;
    QuoteView bid;
    QuoteView ask;
    std::vector<TradeView> trades;
};

inline nlohmann::json quote_json(const QuoteView& quote) {
    if (!quote.present) {
        return nullptr;
    }
    return nlohmann::json{{"price", quote.price}, {"size", quote.size}};
}

inline std::string book_view_json(const BookView& view) {
    nlohmann::json trades = nlohmann::json::array();
    for (const TradeView& trade : view.trades) {
        trades.push_back(nlohmann::json{{"side", trade.side},
                                        {"price", trade.price},
                                        {"size", trade.size},
                                        {"trade_id", trade.trade_id}});
    }
    const nlohmann::json root{{"product_id", view.product_id},
                              {"book_live", view.book_live},
                              {"bid", quote_json(view.bid)},
                              {"ask", quote_json(view.ask)},
                              {"trades", trades}};
    return root.dump();
}

}  // namespace trdqueue
