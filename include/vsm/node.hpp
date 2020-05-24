#pragma once

#include <string>

#include <memory>

namespace vsm {

struct Message2 {
    std::unique_ptr<uint8_t> buf;
};

class Node {
public:
    struct Config {
        uint16_t beacon_interval = 1000;
    };

    struct Peer {};

    void link(const std::string& addr);
    void unlink(const std::string& addr);
};

}  // namespace vsm
