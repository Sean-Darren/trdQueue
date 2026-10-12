#pragma once

#include <string>

class LocalMarket {
public:
    std::string submit_json(const std::string& body);
    std::string cancel_json(const std::string& body);
    std::string snapshot_json();
};
