#pragma once

#include "trdqueue/decimal.hpp"
#include "trdqueue/types.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace trdqueue {

enum class ExchangeMsgType {
    Snapshot,
    L2Update,
    Heartbeat,
    Subscriptions,
    Error,
    Other
};

struct ExchangeMessage {
    ExchangeMsgType type{};
    std::string type_name;
    std::string product_id;
    std::string detail;
    std::vector<BookLevel> bids;
    std::vector<BookLevel> asks;
    std::vector<LevelUpdate> changes;
};

struct ParseOutcome {
    bool ok = false;
    std::string error;
    ExchangeMessage message;
};

namespace coinbase_detail {

inline bool read_ticks(const nlohmann::json& value,
                       std::int64_t& out,
                       const char* what,
                       std::string& error) {
    if (!value.is_string()) {
        error = std::string(what) + " is not a string";
        return false;
    }
    const std::string text = value.get<std::string>();
    const std::optional<std::int64_t> parsed = parse_decimal_to_ticks(text);
    if (!parsed) {
        error = std::string("invalid ") + what + " '" + text + "'";
        return false;
    }
    out = *parsed;
    return true;
}

inline bool read_side(const nlohmann::json& value, Side& out, std::string& error) {
    if (!value.is_string()) {
        error = "side is not a string";
        return false;
    }
    const std::string text = value.get<std::string>();
    if (text == "buy") {
        out = Side::Buy;
        return true;
    }
    if (text == "sell") {
        out = Side::Sell;
        return true;
    }
    error = "invalid side '" + text + "' (Exchange l2update uses buy/sell)";
    return false;
}

inline bool read_levels(const nlohmann::json& arr,
                        std::vector<BookLevel>& out,
                        std::string& error) {
    if (!arr.is_array()) {
        error = "price levels are not an array";
        return false;
    }
    out.clear();
    out.reserve(arr.size());
    for (const auto& tuple : arr) {
        if (!tuple.is_array() || tuple.size() < 2) {
            error = "price level must be [price, size]";
            return false;
        }
        BookLevel level{};
        if (!read_ticks(tuple.at(0), level.price_ticks, "price", error) ||
            !read_ticks(tuple.at(1), level.size_ticks, "size", error)) {
            return false;
        }
        out.push_back(level);
    }
    return true;
}

inline bool read_changes(const nlohmann::json& arr,
                         std::vector<LevelUpdate>& out,
                         std::string& error) {
    if (!arr.is_array()) {
        error = "changes are not an array";
        return false;
    }
    out.clear();
    out.reserve(arr.size());
    for (const auto& change : arr) {
        if (!change.is_array() || change.size() < 3) {
            error = "change must be [side, price, size]";
            return false;
        }
        LevelUpdate update{};
        if (!read_side(change.at(0), update.side, error) ||
            !read_ticks(change.at(1), update.price_ticks, "price", error) ||
            !read_ticks(change.at(2), update.size_ticks, "size", error)) {
            return false;
        }
        out.push_back(update);
    }
    return true;
}

inline void read_product_id(const nlohmann::json& msg, ExchangeMessage& out) {
    if (msg.contains("product_id") && msg["product_id"].is_string()) {
        out.product_id = msg["product_id"].get<std::string>();
    }
}

}  // namespace coinbase_detail

inline ParseOutcome parse_exchange_message(const nlohmann::json& msg) {
    ParseOutcome out;
    if (!msg.is_object() || !msg.contains("type") || !msg["type"].is_string()) {
        out.error = "message has no string type";
        return out;
    }

    out.message.type_name = msg["type"].get<std::string>();
    coinbase_detail::read_product_id(msg, out.message);
    const std::string& type = out.message.type_name;

    if (type == "heartbeat") {
        out.ok = true;
        out.message.type = ExchangeMsgType::Heartbeat;
        return out;
    }
    if (type == "subscriptions") {
        out.ok = true;
        out.message.type = ExchangeMsgType::Subscriptions;
        return out;
    }
    if (type == "error") {
        out.ok = true;
        out.message.type = ExchangeMsgType::Error;
        if (msg.contains("message") && msg["message"].is_string()) {
            out.message.detail = msg["message"].get<std::string>();
        } else if (msg.contains("reason") && msg["reason"].is_string()) {
            out.message.detail = msg["reason"].get<std::string>();
        }
        return out;
    }
    if (type == "snapshot") {
        if (!msg.contains("bids") || !msg.contains("asks")) {
            out.error = "snapshot is missing bids or asks";
            return out;
        }
        if (!coinbase_detail::read_levels(msg["bids"], out.message.bids, out.error) ||
            !coinbase_detail::read_levels(msg["asks"], out.message.asks, out.error)) {
            return out;
        }
        out.ok = true;
        out.message.type = ExchangeMsgType::Snapshot;
        return out;
    }
    if (type == "l2update") {
        if (!msg.contains("changes")) {
            out.error = "l2update is missing changes";
            return out;
        }
        if (!coinbase_detail::read_changes(msg["changes"], out.message.changes, out.error)) {
            return out;
        }
        out.ok = true;
        out.message.type = ExchangeMsgType::L2Update;
        return out;
    }

    out.ok = true;
    out.message.type = ExchangeMsgType::Other;
    return out;
}

}  // namespace trdqueue