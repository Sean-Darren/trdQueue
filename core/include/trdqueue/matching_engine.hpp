#pragma once

#include "trdqueue/types.hpp"

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace trdqueue {

struct RestingOrder {
    std::uint64_t id{};
    Side side{};
    std::int64_t price_ticks{};
    std::int64_t size_ticks{};
};

struct Execution {
    bool accepted = false;
    std::uint64_t order_id = 0;
    std::int64_t resting_size = 0;
    std::vector<Trade> fills;
};

class MatchingEngine {
public:
    Execution submit(Side side, std::int64_t price_ticks, std::int64_t size_ticks);
    bool cancel(std::uint64_t order_id);

    std::optional<BookLevel> best_bid() const;
    std::optional<BookLevel> best_ask() const;

    std::vector<RestingOrder> bids() const;
    std::vector<RestingOrder> asks() const;

private:
    struct Order {
        std::uint64_t id{};
        Side side{};
        std::int64_t price_ticks{};
        std::int64_t size_ticks{};
    };

    using BidBook = std::map<std::int64_t, std::list<Order>, std::greater<std::int64_t>>;
    using AskBook = std::map<std::int64_t, std::list<Order>>;

    template <typename Book>
    void rest(Book& book, const Order& order);

    template <typename Book>
    bool erase_level_order(Book& book, std::uint64_t order_id, std::int64_t price_ticks);

    template <typename Book>
    void match(Book& book,
               Side maker_side,
               std::int64_t incoming_price,
               std::int64_t& remaining,
               std::vector<Trade>& fills,
               bool incoming_is_buy);

    template <typename Book>
    std::vector<RestingOrder> list_orders(const Book& book) const;

    template <typename Book>
    std::optional<BookLevel> best_level(const Book& book) const;

    BidBook bids_;
    AskBook asks_;
    std::unordered_map<std::uint64_t, std::int64_t> price_of_;
    std::uint64_t next_order_id_{1};
    std::int64_t next_trade_id_{1};
};

}  // namespace trdqueue
