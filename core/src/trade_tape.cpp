#include "trdqueue/trade_tape.hpp"

#include <cassert>

namespace trdqueue {

TradeTape::TradeTape(std::size_t capacity) : buf_(capacity) {}

void TradeTape::push(const Trade& trade) {
    if (buf_.empty()) {
        return;
    }
    buf_[next_] = trade;
    next_ = (next_ + 1) % buf_.size();
    if (count_ < buf_.size()) {
        ++count_;
    }
}

const Trade& TradeTape::at(std::size_t i) const {
    assert(i < count_);
    const std::size_t cap = buf_.size();
    const std::size_t start = (count_ == cap) ? next_ : 0;
    return buf_[(start + i) % cap];
}

}
