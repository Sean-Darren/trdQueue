#pragma once

#include "trdqueue/types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace trdqueue {

class OrderBook {
public:
    void apply_snapshot(const std::vector<BookLevel>& bids,
                        const std::vector<BookLevel>& asks);

    void apply_level_update(const LevelUpdate& update);

    std::optional<BookLevel> best_bid() const;
    std::optional<BookLevel> best_ask() const;

    std::size_t bid_count() const { return bids_.size(); }
    std::size_t ask_count() const { return asks_.size(); }

    std::int64_t size_at(Side side, std::int64_t price_ticks) const;

private:
    std::map<std::int64_t, std::int64_t, std::greater<std::int64_t>> bids_;
    std::map<std::int64_t, std::int64_t> asks_;
};

}
