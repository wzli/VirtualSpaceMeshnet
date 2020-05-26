#pragma once

#include <vsm/logger.hpp>
#include <vsm/peer_manager.hpp>
#include <vsm/transport.hpp>

namespace vsm {

class MeshNode {
public:
    enum ErrorType {
        ADD_MESSAGE_HANDLER_FAIL,
        ADD_TIMER_FAIL,
        MESSAGE_VERIFY_FAIL,
    };

    struct Config {
        uint16_t beacon_interval = 1000;
        std::string node_name = "node";
        Vector2 start_coordinates = {0, 0};
        Logger::Level log_level = Logger::FATAL;
        Logger::LogHandler log_handler = nullptr;
    };

    struct Stats {
        uint32_t peer_updates_received;
        uint32_t state_updates_received;
        uint32_t message_verify_failures;
    };

    MeshNode(Config config, std::unique_ptr<Transport> transport);

    Logger& getLogger() { return _logger; }
    PeerManager& getPeerManager() { return _peer_manager; }
    Transport& getTransport() { return *_transport; }

    const Stats& getStats() const { return _stats; }

private:
    const Message* getMessage(const void* buffer, size_t len);
    void recvPeerUpdates(const void* buffer, size_t len);
    void recvStateUpdates(const void* buffer, size_t len);

    Stats _stats;
    Logger _logger;
    PeerManager _peer_manager;
    std::unique_ptr<Transport> _transport;
};

}  // namespace vsm
