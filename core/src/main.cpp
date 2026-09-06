#include "trdqueue/decimal.hpp"
#include "trdqueue/order_book.hpp"
#include "trdqueue/spsc_ring_buffer.hpp"
#include "trdqueue/types.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using trdqueue::BookLevel;
using trdqueue::Event;
using trdqueue::EventType;
using trdqueue::LevelUpdate;
using trdqueue::OrderBook;
using trdqueue::Side;
using trdqueue::SpscRingBuffer;
using trdqueue::parse_decimal_to_ticks;
using trdqueue::ticks_to_decimal;

namespace {

[[noreturn]] void die(const std::string& msg) {
    std::cerr << "error: " << msg << '\n';
    std::exit(1);
}

std::int64_t require_ticks(const std::string& s, const char* what) {
    const auto v = parse_decimal_to_ticks(s);
    if (!v) {
        die(std::string("invalid ") + what + " '" + s + "'");
    }
    return *v;
}

Side require_side(const std::string& s) {
    if (s == "buy") {
        return Side::Buy;
    }
    if (s == "sell") {
        return Side::Sell;
    }
    die("invalid side '" + s + "' (Exchange l2update uses buy/sell)");
}

std::vector<BookLevel> parse_levels(const nlohmann::json& arr) {
    std::vector<BookLevel> out;
    out.reserve(arr.size());
    for (const auto& tuple : arr) {
        BookLevel level{};
        level.price_ticks = require_ticks(tuple.at(0).get<std::string>(), "price");
        level.size_ticks = require_ticks(tuple.at(1).get<std::string>(), "size");
        out.push_back(level);
    }
    return out;
}

void print_bbo(const OrderBook& book) {
    const auto bid = book.best_bid();
    const auto ask = book.best_ask();
    if (bid) {
        std::cout << "best_bid " << ticks_to_decimal(bid->price_ticks) << " x "
                  << ticks_to_decimal(bid->size_ticks) << '\n';
    } else {
        std::cout << "best_bid (none)\n";
    }
    if (ask) {
        std::cout << "best_ask " << ticks_to_decimal(ask->price_ticks) << " x "
                  << ticks_to_decimal(ask->size_ticks) << '\n';
    } else {
        std::cout << "best_ask (none)\n";
    }
}

}

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : TRDQUEUE_FIXTURE_PATH;
    std::ifstream in(path);
    if (!in) {
        die(std::string("cannot open fixture ") + path);
    }

    nlohmann::json root;
    in >> root;

    OrderBook book;
    SpscRingBuffer<Event, 1024> ring;
    bool saw_snapshot = false;

    for (const auto& msg : root.at("messages")) {
        const std::string type = msg.at("type").get<std::string>();
        if (type == "snapshot") {
            book.apply_snapshot(parse_levels(msg.at("bids")),
                                parse_levels(msg.at("asks")));
            saw_snapshot = true;
            continue;
        }
        if (type != "l2update") {
            die("unsupported fixture message type '" + type + "'");
        }
        if (!saw_snapshot) {
            die("l2update before snapshot");
        }
        for (const auto& change : msg.at("changes")) {
            LevelUpdate u{};
            u.side = require_side(change.at(0).get<std::string>());
            u.price_ticks = require_ticks(change.at(1).get<std::string>(), "price");
            u.size_ticks = require_ticks(change.at(2).get<std::string>(), "size");
            if (!ring.try_push(trdqueue::make_level_event(u))) {
                die("ring dropped an incremental update (fixture is tiny; this is a bug)");
            }
        }
    }

    Event e{};
    while (ring.try_pop(e)) {
        if (e.type != EventType::LevelUpdate) {
            die("unexpected event type from ring");
        }
        book.apply_level_update(trdqueue::as_level_update(e));
    }

    print_bbo(book);
    return 0;
}
