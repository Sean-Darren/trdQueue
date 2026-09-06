#include "trdqueue/order_book.hpp"

namespace trdqueue {

void OrderBook::apply_snapshot(const std::vector<BookLevel>& bids,
                               const std::vector<BookLevel>& asks) {
    bids_.clear();
    asks_.clear();
    for (const BookLevel& level : bids) {
        if (level.size_ticks > 0) {
            bids_[level.price_ticks] = level.size_ticks;
        }
    }
    for (const BookLevel& level : asks) {
        if (level.size_ticks > 0) {
            asks_[level.price_ticks] = level.size_ticks;
        }
    }
}

void OrderBook::apply_level_update(const LevelUpdate& update) {
    if (update.side == Side::Buy) {
        if (update.size_ticks == 0) {
            bids_.erase(update.price_ticks);
        } else {
            bids_[update.price_ticks] = update.size_ticks;
        }
    } else {
        if (update.size_ticks == 0) {
            asks_.erase(update.price_ticks);
        } else {
            asks_[update.price_ticks] = update.size_ticks;
        }
    }
}

std::optional<BookLevel> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    const auto it = bids_.begin();
    return BookLevel{it->first, it->second};
}

std::optional<BookLevel> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    const auto it = asks_.begin();
    return BookLevel{it->first, it->second};
}

std::int64_t OrderBook::size_at(Side side, std::int64_t price_ticks) const {
    if (side == Side::Buy) {
        const auto it = bids_.find(price_ticks);
        return it == bids_.end() ? 0 : it->second;
    }
    const auto it = asks_.find(price_ticks);
    return it == asks_.end() ? 0 : it->second;
}

}
