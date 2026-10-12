#include "book_view.hpp"
#include "coinbase_parse.hpp"
#include "dashboard.hpp"
#include "local_market.hpp"
#include "trdqueue/decimal.hpp"
#include "trdqueue/order_book.hpp"
#include "trdqueue/spsc_ring_buffer.hpp"
#include "trdqueue/trade_tape.hpp"
#include "trdqueue/types.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cctype>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> g_stop{false};

extern "C" void on_signal(int) {
    g_stop.store(true, std::memory_order_relaxed);
}

constexpr const char* kFeedUrl = "wss://ws-feed.exchange.coinbase.com";
constexpr std::int64_t kSilenceLimitMs = 5000;
constexpr std::size_t kTapeCapacity = 32;

struct BookMsg {
    std::uint64_t epoch = 0;
    bool snapshot = false;
    std::vector<trdqueue::BookLevel> bids;
    std::vector<trdqueue::BookLevel> asks;
    std::vector<trdqueue::LevelUpdate> changes;
};

struct Top {
    bool has_bid = false;
    bool has_ask = false;
    std::int64_t bid_px = 0;
    std::int64_t bid_sz = 0;
    std::int64_t ask_px = 0;
    std::int64_t ask_sz = 0;
};

bool operator==(const Top& a, const Top& b) {
    return a.has_bid == b.has_bid && a.has_ask == b.has_ask && a.bid_px == b.bid_px &&
           a.bid_sz == b.bid_sz && a.ask_px == b.ask_px && a.ask_sz == b.ask_sz;
}

bool operator!=(const Top& a, const Top& b) {
    return !(a == b);
}

bool valid_product(const std::string& product) {
    if (product.empty() || product.size() > 32) {
        return false;
    }
    for (unsigned char c : product) {
        if (!(std::isalnum(c) || c == '-')) {
            return false;
        }
    }
    return true;
}

Top capture(const trdqueue::OrderBook& book) {
    Top top;
    if (const auto bid = book.best_bid()) {
        top.has_bid = true;
        top.bid_px = bid->price_ticks;
        top.bid_sz = bid->size_ticks;
    }
    if (const auto ask = book.best_ask()) {
        top.has_ask = true;
        top.ask_px = ask->price_ticks;
        top.ask_sz = ask->size_ticks;
    }
    return top;
}

std::string format_level(bool has, std::int64_t price, std::int64_t size) {
    if (!has) {
        return "(none)";
    }
    return trdqueue::ticks_to_decimal(price) + " x " + trdqueue::ticks_to_decimal(size);
}

trdqueue::BookView make_view(const std::string& product,
                             bool book_live,
                             const Top& top,
                             const trdqueue::TradeTape& tape) {
    trdqueue::BookView view;
    view.product_id = product;
    view.book_live = book_live;
    if (top.has_bid) {
        view.bid.present = true;
        view.bid.price = trdqueue::ticks_to_decimal(top.bid_px);
        view.bid.size = trdqueue::ticks_to_decimal(top.bid_sz);
    }
    if (top.has_ask) {
        view.ask.present = true;
        view.ask.price = trdqueue::ticks_to_decimal(top.ask_px);
        view.ask.size = trdqueue::ticks_to_decimal(top.ask_sz);
    }
    view.trades.reserve(tape.size());
    for (std::size_t i = tape.size(); i > 0; --i) {
        const trdqueue::Trade& trade = tape.at(i - 1);
        trdqueue::TradeView row;
        row.side = trade.side == trdqueue::Side::Buy ? "buy" : "sell";
        row.price = trdqueue::ticks_to_decimal(trade.price_ticks);
        row.size = trdqueue::ticks_to_decimal(trade.size_ticks);
        row.trade_id = trade.trade_id;
        view.trades.push_back(std::move(row));
    }
    return view;
}

class LiveSession {
public:
    LiveSession(std::string product,
                trdqueue::SpscRingBuffer<BookMsg*, 1024>& ring,
                trdqueue::SpscRingBuffer<trdqueue::Trade, 1024>& trades)
        : product_(std::move(product)), ring_(ring), trades_(trades), tape_(kTapeCapacity) {}

    bool start() {
        if (!ix::initNetSystem()) {
            std::cerr << "error: network init failed\n";
            return false;
        }
        net_ready_ = true;
        dashboard_.set_order_handler(
            [this](const std::string& body) { return local_.submit_json(body); });
        dashboard_.set_cancel_handler(
            [this](const std::string& body) { return local_.cancel_json(body); });
        dashboard_.set_local_snapshot([this]() { return local_.snapshot_json(); });
        if (!dashboard_.start()) {
            ix::uninitNetSystem();
            net_ready_ = false;
            return false;
        }
        publish();
        ws_.setUrl(kFeedUrl);
        ws_.setHandshakeTimeout(10);
        ws_.setOnMessageCallback(
            [this](const ix::WebSocketMessagePtr& msg) { on_message(msg); });
        ws_.enableAutomaticReconnection();
        std::cerr << "connecting to " << kFeedUrl << ' ' << product_ << '\n';
        ws_.start();
        started_ = true;
        return true;
    }

    void shutdown() {
        g_stop.store(true);
        if (started_) {
            ws_.disableAutomaticReconnection();
            ws_.stop();
            started_ = false;
        }
        drain();
        dashboard_.stop();
        if (net_ready_) {
            ix::uninitNetSystem();
            net_ready_ = false;
        }
    }

    void pump_once() {
        check_silence();
        bool worked = false;
        BookMsg* msg = nullptr;
        while (ring_.try_pop(msg)) {
            handle(msg);
            delete msg;
            msg = nullptr;
            worked = true;
        }
        trdqueue::Trade trade{};
        while (trades_.try_pop(trade)) {
            on_trade(trade);
            worked = true;
        }
        if (!worked) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

private:
    void touch_rx() {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
        last_rx_ms_.store(ms, std::memory_order_relaxed);
    }

    void request_reconnect(const std::string& why) {
        accept_updates_.store(false, std::memory_order_release);
        if (g_stop.load(std::memory_order_acquire)) {
            return;
        }
        bool expected = false;
        if (!resync_pending_.compare_exchange_strong(expected, true)) {
            return;
        }
        epoch_.fetch_add(1, std::memory_order_acq_rel);
        socket_open_.store(false, std::memory_order_release);
        std::cerr << "reconnect: " << why << '\n';
        ws_.close();
    }

    void on_open() {
        resync_pending_.store(false, std::memory_order_release);
        accept_updates_.store(false, std::memory_order_release);
        const auto id = epoch_.fetch_add(1, std::memory_order_acq_rel) + 1;
        publish_epoch_.store(id, std::memory_order_release);
        socket_open_.store(true, std::memory_order_release);
        touch_rx();
        std::cerr << "connected, subscribing to " << product_ << '\n';

        nlohmann::json sub;
        sub["type"] = "subscribe";
        sub["product_ids"] = nlohmann::json::array({product_});
        sub["channels"] = nlohmann::json::array({"level2_batch", "heartbeat", "matches"});
        const ix::WebSocketSendInfo info = ws_.send(sub.dump());
        if (!info.success) {
            request_reconnect("failed to send subscribe");
        }
    }

    bool enqueue(std::unique_ptr<BookMsg> msg, bool snapshot) {
        const auto id = publish_epoch_.load(std::memory_order_acquire);
        if (id != epoch_.load(std::memory_order_acquire)) {
            return false;
        }
        msg->epoch = id;
        BookMsg* raw = msg.get();
        if (!ring_.try_push(raw)) {
            request_reconnect("ring full; book is stale");
            return false;
        }
        msg.release();
        if (snapshot && epoch_.load(std::memory_order_acquire) == id) {
            accept_updates_.store(true, std::memory_order_release);
        }
        return true;
    }

    void on_book_message(const trdqueue::ExchangeMessage& message) {
        if (message.product_id != product_) {
            if (message.product_id.empty()) {
                request_reconnect("book message missing product_id");
            }
            return;
        }
        if (message.type == trdqueue::ExchangeMsgType::L2Update &&
            !accept_updates_.load(std::memory_order_acquire)) {
            return;
        }

        auto booked = std::make_unique<BookMsg>();
        booked->snapshot = message.type == trdqueue::ExchangeMsgType::Snapshot;
        booked->bids = message.bids;
        booked->asks = message.asks;
        booked->changes = message.changes;
        const bool snapshot = booked->snapshot;
        enqueue(std::move(booked), snapshot);
    }

    void on_trade_message(const trdqueue::ExchangeMessage& message) {
        if (message.product_id != product_) {
            return;
        }
        trades_.try_push(message.trade);
    }

    void on_message(const ix::WebSocketMessagePtr& msg) {
        if (g_stop.load(std::memory_order_acquire)) {
            return;
        }
        if (msg->type == ix::WebSocketMessageType::Open) {
            on_open();
            return;
        }
        if (msg->type == ix::WebSocketMessageType::Close) {
            socket_open_.store(false, std::memory_order_release);
            request_reconnect("socket closed");
            return;
        }
        if (msg->type == ix::WebSocketMessageType::Error) {
            request_reconnect(msg->errorInfo.reason.empty() ? "socket error"
                                                            : msg->errorInfo.reason);
            return;
        }
        if (msg->type != ix::WebSocketMessageType::Message) {
            return;
        }

        touch_rx();
        nlohmann::json root;
        try {
            root = nlohmann::json::parse(msg->str);
        } catch (const nlohmann::json::exception& ex) {
            request_reconnect(std::string("json parse error: ") + ex.what());
            return;
        }

        const trdqueue::ParseOutcome parsed = trdqueue::parse_exchange_message(root);
        if (!parsed.ok) {
            request_reconnect(parsed.error);
            return;
        }
        using trdqueue::ExchangeMsgType;
        if (parsed.message.type == ExchangeMsgType::Heartbeat ||
            parsed.message.type == ExchangeMsgType::Subscriptions ||
            parsed.message.type == ExchangeMsgType::Other) {
            return;
        }
        if (parsed.message.type == ExchangeMsgType::Error) {
            const std::string& detail = parsed.message.detail;
            request_reconnect(detail.empty() ? "exchange error" : detail);
            return;
        }
        if (parsed.message.type == ExchangeMsgType::Trade) {
            on_trade_message(parsed.message);
            return;
        }
        on_book_message(parsed.message);
    }

    void check_silence() {
        if (!socket_open_.load(std::memory_order_acquire) ||
            g_stop.load(std::memory_order_acquire)) {
            return;
        }
        const auto last = last_rx_ms_.load(std::memory_order_relaxed);
        if (last == 0) {
            return;
        }
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();
        if (now - last > kSilenceLimitMs) {
            request_reconnect("no feed message for 5s");
        }
    }

    void print_top() const {
        std::cout << product_ << "  bid "
                  << format_level(last_top_.has_bid, last_top_.bid_px, last_top_.bid_sz)
                  << "  ask "
                  << format_level(last_top_.has_ask, last_top_.ask_px, last_top_.ask_sz)
                  << '\n';
        std::cout.flush();
    }

    void publish() {
        dashboard_.publish(make_view(product_, book_live_, last_top_, tape_));
    }

    void on_trade(const trdqueue::Trade& trade) {
        if (saw_trade_ && trade.trade_id == last_trade_id_) {
            return;
        }
        tape_.push(trade);
        last_trade_id_ = trade.trade_id;
        saw_trade_ = true;
        publish();
    }

    void handle(BookMsg* msg) {
        if (msg->epoch != epoch_.load(std::memory_order_acquire)) {
            if (book_live_) {
                book_live_ = false;
                publish();
            }
            return;
        }
        if (msg->snapshot) {
            book_.apply_snapshot(msg->bids, msg->asks);
            book_live_ = true;
            last_top_ = capture(book_);
            print_top();
            publish();
            return;
        }
        if (!book_live_) {
            return;
        }
        for (const auto& update : msg->changes) {
            book_.apply_level_update(update);
        }
        const Top top = capture(book_);
        if (top != last_top_) {
            last_top_ = top;
            print_top();
            publish();
        }
    }

    void drain() {
        BookMsg* msg = nullptr;
        while (ring_.try_pop(msg)) {
            if (msg->epoch == epoch_.load(std::memory_order_acquire)) {
                handle(msg);
            }
            delete msg;
        }
        trdqueue::Trade trade{};
        while (trades_.try_pop(trade)) {
            on_trade(trade);
        }
    }

    std::string product_;
    trdqueue::SpscRingBuffer<BookMsg*, 1024>& ring_;
    trdqueue::SpscRingBuffer<trdqueue::Trade, 1024>& trades_;
    ix::WebSocket ws_;
    LocalMarket local_;
    Dashboard dashboard_;
    trdqueue::OrderBook book_;
    trdqueue::TradeTape tape_;
    Top last_top_{};
    bool book_live_ = false;
    bool saw_trade_ = false;
    std::int64_t last_trade_id_ = 0;
    bool started_ = false;
    bool net_ready_ = false;
    std::atomic<bool> socket_open_{false};
    std::atomic<bool> accept_updates_{false};
    std::atomic<bool> resync_pending_{false};
    std::atomic<std::uint64_t> epoch_{0};
    std::atomic<std::uint64_t> publish_epoch_{0};
    std::atomic<std::int64_t> last_rx_ms_{0};
};

}

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "usage: trdqueue_live [PRODUCT_ID]\n";
        return 2;
    }
    const std::string product = argc == 2 ? argv[1] : "BTC-USD";
    if (!valid_product(product)) {
        std::cerr << "error: invalid product id '" << product << "'\n";
        return 2;
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    trdqueue::SpscRingBuffer<BookMsg*, 1024> ring;
    trdqueue::SpscRingBuffer<trdqueue::Trade, 1024> trades;
    LiveSession session(product, ring, trades);
    if (!session.start()) {
        return 1;
    }
    while (!g_stop.load(std::memory_order_acquire)) {
        session.pump_once();
    }
    session.shutdown();
    return 0;
}