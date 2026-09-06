#pragma once

#include "trdqueue/types.hpp"

#include <cstddef>
#include <vector>

namespace trdqueue {

class TradeTape {
public:
    explicit TradeTape(std::size_t capacity);

    void push(const Trade& trade);

    std::size_t size() const { return count_; }
    std::size_t capacity() const { return buf_.size(); }

    const Trade& at(std::size_t i) const;

private:
    std::vector<Trade> buf_;
    std::size_t next_{0};
    std::size_t count_{0};
};

}
