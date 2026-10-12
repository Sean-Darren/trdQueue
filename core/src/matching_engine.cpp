#include "trdqueue/matching_engine.hpp"

#include <algorithm>
#include <utility>

namespace trdqueue {

template <typename Book>
void MatchingEngine::rest(Book& book, const Order& order) {
    auto& queue = book[order.price_ticks];
    queue.push_back(order);
    price_of_[order.id] = order.price_ticks;
}

template <typename Book>
bool MatchingEngine::erase_level_order(Book& book,
                                       std::uint64_t order_id,
                                       std::int64_t price_ticks) {
    const auto level = book.find(price_ticks);
    if (level == book.end()) {
        return false;
    }
    auto& queue = level->second;
    const auto it = std::find_if(queue.begin(), queue.end(), [order_id](const Order& order) {
        return order.id == order_id;
    });
    if (it == queue.end()) {
        return false;
    }
    queue.erase(it);
    price_of_.erase(order_id);
    if (queue.empty()) {
        book.erase(level);
    }
    return true;
}

template <typename Book>
void MatchingEngine::match(Book& book,
                           Side maker_side,
                           std::int64_t incoming_price,
                           std::int64_t& remaining,
                           std::vector<Trade>& fills,
                           bool incoming_is_buy) {
    while (remaining > 0 && !book.empty()) {
        auto level = book.begin();
        const std::int64_t resting_price = level->first;
        const bool marketable =
            incoming_is_buy ? resting_price <= incoming_price : resting_price >= incoming_price;
        if (!marketable) {
            break;
        }
        auto& queue = level->second;
        Order& maker = queue.front();
        const std::int64_t fill = std::min(remaining, maker.size_ticks);
        Trade trade{};
        trade.side = maker_side;
        trade.price_ticks = maker.price_ticks;
        trade.size_ticks = fill;
        trade.trade_id = next_trade_id_++;
        fills.push_back(trade);
        remaining -= fill;
        maker.size_ticks -= fill;
        if (maker.size_ticks == 0) {
            const std::uint64_t filled_id = maker.id;
            price_of_.erase(filled_id);
            queue.pop_front();
            if (queue.empty()) {
                book.erase(level);
            }
        }
    }
}

template <typename Book>
std::vector<RestingOrder> MatchingEngine::list_orders(const Book& book) const {
    std::vector<RestingOrder> out;
    for (const auto& level : book) {
        for (const Order& order : level.second) {
            out.push_back(RestingOrder{order.id, order.side, order.price_ticks, order.size_ticks});
        }
    }
    return out;
}

template <typename Book>
std::optional<BookLevel> MatchingEngine::best_level(const Book& book) const {
    if (book.empty()) {
        return std::nullopt;
    }
    const auto& queue = book.begin()->second;
    std::int64_t size = 0;
    for (const Order& order : queue) {
        size += order.size_ticks;
    }
    return BookLevel{book.begin()->first, size};
}

Execution MatchingEngine::submit(Side side, std::int64_t price_ticks, std::int64_t size_ticks) {
    Execution result;
    if (price_ticks <= 0 || size_ticks <= 0) {
        return result;
    }
    result.accepted = true;
    result.order_id = next_order_id_++;
    std::int64_t remaining = size_ticks;
    if (side == Side::Buy) {
        match(asks_, Side::Sell, price_ticks, remaining, result.fills, true);
    } else {
        match(bids_, Side::Buy, price_ticks, remaining, result.fills, false);
    }
    if (remaining > 0) {
        const Order resting{result.order_id, side, price_ticks, remaining};
        if (side == Side::Buy) {
            rest(bids_, resting);
        } else {
            rest(asks_, resting);
        }
        result.resting_size = remaining;
    }
    return result;
}

bool MatchingEngine::cancel(std::uint64_t order_id) {
    const auto found = price_of_.find(order_id);
    if (found == price_of_.end()) {
        return false;
    }
    const std::int64_t price = found->second;
    if (erase_level_order(bids_, order_id, price)) {
        return true;
    }
    return erase_level_order(asks_, order_id, price);
}

std::optional<BookLevel> MatchingEngine::best_bid() const {
    return best_level(bids_);
}

std::optional<BookLevel> MatchingEngine::best_ask() const {
    return best_level(asks_);
}

std::vector<RestingOrder> MatchingEngine::bids() const {
    return list_orders(bids_);
}

std::vector<RestingOrder> MatchingEngine::asks() const {
    return list_orders(asks_);
}

}  // namespace trdqueue
