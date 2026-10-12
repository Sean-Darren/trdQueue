#pragma once

#include "book_view.hpp"

#include <ixwebsocket/IXHttpServer.h>

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

private:
    ix::HttpResponsePtr handle_http(ix::HttpRequestPtr request) const;
    std::string snapshot_json() const;
    void broadcast(const std::string& payload);

    ix::HttpServer server_;
    mutable std::mutex mu_;
    trdqueue::BookView view_{};
    bool started_ = false;
};
