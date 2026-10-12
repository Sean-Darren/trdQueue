#include "local_market.hpp"

#include "local_book.hpp"
#include "trdqueue/decimal.hpp"
#include "trdqueue/matching_engine.hpp"
#include "trdqueue/trade_tape.hpp"

#include <nlohmann/json.hpp>

#include <mutex>

namespace {

constexpr std::size_t kLocalFills = 32;

trdqueue::TradeView trade_view(const trdqueue::Trade& trade) {
    trdqueue::TradeView row;
    row.side = trade.side == trdqueue::Side::Buy ? "buy" : "sell";
    row.price = trdqueue::ticks_to_decimal(trade.price_ticks);
    row.size = trdqueue::ticks_to_decimal(trade.size_ticks);
    row.trade_id = trade.trade_id;
    return row;
}

class BookState {
public:
    std::string submit(const std::string& body) {
        std::lock_guard<std::mutex> lock(mu_);
        nlohmann::json parsed;
        try {
            parsed = nlohmann::json::parse(body);
        } catch (const nlohmann::json::exception&) {
            return result_json(false, {}, "order must be JSON");
        }
        if (!parsed.is_object() || !parsed.contains("side") || !parsed["side"].is_string() ||
            !parsed.contains("price") || !parsed["price"].is_string() || !parsed.contains("size") ||
            !parsed["size"].is_string()) {
            return result_json(false, {}, "side, price, and size are required");
        }
        const std::string side_text = parsed["side"].get<std::string>();
        trdqueue::Side side{};
        if (side_text == "buy") {
            side = trdqueue::Side::Buy;
        } else if (side_text == "sell") {
            side = trdqueue::Side::Sell;
        } else {
            return result_json(false, {}, "side must be buy or sell");
        }
        const auto price = trdqueue::parse_decimal_to_ticks(parsed["price"].get<std::string>());
        const auto size = trdqueue::parse_decimal_to_ticks(parsed["size"].get<std::string>());
        if (!price || !size) {
            return result_json(false, {}, "price and size must be decimal numbers");
        }
        const trdqueue::Execution execution = engine_.submit(side, *price, *size);
        if (!execution.accepted) {
            return result_json(false, execution, "price and size must be positive");
        }
        for (const trdqueue::Trade& fill : execution.fills) {
            fills_.push(fill);
        }
        std::string message = "resting";
        if (!execution.fills.empty() && execution.resting_size > 0) {
            message = "partially filled, resting " +
                      trdqueue::ticks_to_decimal(execution.resting_size);
        } else if (!execution.fills.empty()) {
            message = "filled";
        }
        return result_json(true, execution, message);
    }

    std::string cancel(const std::string& body) {
        std::lock_guard<std::mutex> lock(mu_);
        nlohmann::json parsed;
        try {
            parsed = nlohmann::json::parse(body);
        } catch (const nlohmann::json::exception&) {
            return result_json(false, {}, "cancel must be JSON");
        }
        if (!parsed.is_object() || !parsed.contains("order_id") ||
            !(parsed["order_id"].is_number_integer() || parsed["order_id"].is_number_unsigned())) {
            return result_json(false, {}, "order_id is required");
        }
        const auto order_id = parsed["order_id"].get<std::uint64_t>();
        if (!engine_.cancel(order_id)) {
            return result_json(false, {}, "order is not resting");
        }
        trdqueue::Execution execution;
        execution.order_id = order_id;
        return result_json(true, execution, "cancelled");
    }

    std::string snapshot() {
        std::lock_guard<std::mutex> lock(mu_);
        return result_json(false, {}, "", false);
    }

private:
    std::string result_json(bool accepted,
                            const trdqueue::Execution& execution,
                            const std::string& message,
                            bool include_result = true) const {
        trdqueue::LocalSnapshot snapshot;
        snapshot.include_result = include_result;
        snapshot.accepted = accepted;
        snapshot.order_id = execution.order_id;
        snapshot.resting_size = trdqueue::ticks_to_decimal(execution.resting_size);
        snapshot.message = message;
        for (const trdqueue::RestingOrder& order : engine_.bids()) {
            snapshot.bids.push_back(trdqueue::LocalLevel{
                order.id, trdqueue::ticks_to_decimal(order.price_ticks),
                trdqueue::ticks_to_decimal(order.size_ticks)});
        }
        for (const trdqueue::RestingOrder& order : engine_.asks()) {
            snapshot.asks.push_back(trdqueue::LocalLevel{
                order.id, trdqueue::ticks_to_decimal(order.price_ticks),
                trdqueue::ticks_to_decimal(order.size_ticks)});
        }
        for (const trdqueue::Trade& fill : execution.fills) {
            snapshot.fills.push_back(trade_view(fill));
        }
        for (std::size_t i = fills_.size(); i > 0; --i) {
            snapshot.trades.push_back(trade_view(fills_.at(i - 1)));
        }
        return trdqueue::local_snapshot_json(snapshot);
    }

    std::mutex mu_;
    trdqueue::MatchingEngine engine_;
    trdqueue::TradeTape fills_{kLocalFills};
};

BookState& state() {
    static BookState book;
    return book;
}

}  // namespace

std::string LocalMarket::submit_json(const std::string& body) {
    return state().submit(body);
}

std::string LocalMarket::cancel_json(const std::string& body) {
    return state().cancel(body);
}

std::string LocalMarket::snapshot_json() {
    return state().snapshot();
}
