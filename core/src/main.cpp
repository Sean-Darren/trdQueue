#include "coinbase_parse.hpp"
#include "trdqueue/order_book.hpp"
#include "trdqueue/spsc_ring_buffer.hpp"
#include "trdqueue/types.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using trdqueue::Event;
using trdqueue::EventType;
using trdqueue::ExchangeMsgType;
using trdqueue::OrderBook;
using trdqueue::SpscRingBuffer;
using trdqueue::parse_exchange_message;
using trdqueue::ticks_to_decimal;

namespace {

[[noreturn]] void die(const std::string& msg) {
    std::cerr << "error: " << msg << '\n';
    std::exit(1);
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
        const auto parsed = parse_exchange_message(msg);
        if (!parsed.ok) {
            die(parsed.error);
        }
        if (parsed.message.type == ExchangeMsgType::Snapshot) {
            book.apply_snapshot(parsed.message.bids, parsed.message.asks);
            saw_snapshot = true;
            continue;
        }
        if (parsed.message.type != ExchangeMsgType::L2Update) {
            die("unsupported fixture message type '" + parsed.message.type_name + "'");
        }
        if (!saw_snapshot) {
            die("l2update before snapshot");
        }
        for (const auto& update : parsed.message.changes) {
            if (!ring.try_push(trdqueue::make_level_event(update))) {
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