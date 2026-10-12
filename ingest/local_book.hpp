#pragma once

#include "book_view.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace trdqueue {

struct LocalLevel {
    std::uint64_t id = 0;
    std::string price;
    std::string size;
};

struct LocalSnapshot {
    bool include_result = false;
    bool accepted = false;
    std::uint64_t order_id = 0;
    std::string resting_size;
    std::string message;
    std::vector<LocalLevel> bids;
    std::vector<LocalLevel> asks;
    std::vector<TradeView> fills;
    std::vector<TradeView> trades;
};

inline nlohmann::json levels_json(const std::vector<LocalLevel>& levels) {
    nlohmann::json out = nlohmann::json::array();
    for (const LocalLevel& level : levels) {
        out.push_back(nlohmann::json{
            {"id", level.id}, {"price", level.price}, {"size", level.size}});
    }
    return out;
}

inline nlohmann::json trade_rows_json(const std::vector<TradeView>& trades) {
    nlohmann::json out = nlohmann::json::array();
    for (const TradeView& trade : trades) {
        out.push_back(nlohmann::json{{"side", trade.side},
                                     {"price", trade.price},
                                     {"size", trade.size},
                                     {"trade_id", trade.trade_id}});
    }
    return out;
}

inline std::string local_snapshot_json(const LocalSnapshot& snapshot) {
    nlohmann::json root{{"channel", "local"},
                        {"bids", levels_json(snapshot.bids)},
                        {"asks", levels_json(snapshot.asks)},
                        {"trades", trade_rows_json(snapshot.trades)}};
    if (snapshot.include_result) {
        root["accepted"] = snapshot.accepted;
        root["order_id"] = snapshot.order_id;
        root["resting_size"] = snapshot.resting_size;
        root["message"] = snapshot.message;
        root["fills"] = trade_rows_json(snapshot.fills);
    }
    return root.dump();
}

}  // namespace trdqueue
