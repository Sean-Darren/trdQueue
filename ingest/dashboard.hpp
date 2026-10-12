#pragma once

#include "book_view.hpp"

#include <ixwebsocket/IXHttpServer.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>

class Dashboard {
public:
    Dashboard();
    ~Dashboard();

    Dashboard(const Dashboard&) = delete;
    Dashboard& operator=(const Dashboard&) = delete;

    bool start();
    void stop();
    void publish(const trdqueue::BookView& view);
    void set_order_handler(std::function<std::string(const std::string&)> handler);
    void set_cancel_handler(std::function<std::string(const std::string&)> handler);
    void set_local_snapshot(std::function<std::string()> snapshot);

private:
    ix::HttpResponsePtr handle_http(ix::HttpRequestPtr request);
    std::string snapshot_json() const;
    void broadcast(const std::string& payload);

    ix::HttpServer server_;
    mutable std::mutex mu_;
    trdqueue::BookView view_{};
    bool started_ = false;
    std::function<std::string(const std::string&)> order_handler_;
    std::function<std::string(const std::string&)> cancel_handler_;
    std::function<std::string()> local_snapshot_;
};
