#pragma once

#include <cstdint>

namespace trdqueue {

enum class Side : std::uint8_t { Buy = 0, Sell = 1 };

enum class EventType : std::uint8_t { LevelUpdate = 1, Trade = 2 };

struct LevelUpdate {
    Side side{};
    std::int64_t price_ticks{};
    std::int64_t size_ticks{};
};

struct Trade {
    Side side{};
    std::int64_t price_ticks{};
    std::int64_t size_ticks{};
    std::int64_t trade_id{};
};

struct BookLevel {
    std::int64_t price_ticks{};
    std::int64_t size_ticks{};
};

struct Event {
    EventType type{};
    Side side{};
    std::int64_t price_ticks{};
    std::int64_t size_ticks{};
    std::int64_t trade_id{};
};

inline Event make_level_event(const LevelUpdate& u) {
    Event e{};
    e.type = EventType::LevelUpdate;
    e.side = u.side;
    e.price_ticks = u.price_ticks;
    e.size_ticks = u.size_ticks;
    return e;
}

inline Event make_trade_event(const Trade& t) {
    Event e{};
    e.type = EventType::Trade;
    e.side = t.side;
    e.price_ticks = t.price_ticks;
    e.size_ticks = t.size_ticks;
    e.trade_id = t.trade_id;
    return e;
}

inline LevelUpdate as_level_update(const Event& e) {
    return LevelUpdate{e.side, e.price_ticks, e.size_ticks};
}

inline Trade as_trade(const Event& e) {
    return Trade{e.side, e.price_ticks, e.size_ticks, e.trade_id};
}

}
