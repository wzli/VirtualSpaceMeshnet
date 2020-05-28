#pragma once

#include <vsm/logger.hpp>
#include <vsm/peer_manager.hpp>
#include <vsm/transport.hpp>

namespace vsm {

class MeshNode {
public:
    enum ErrorType {
        START_OFFSET = 100,
        // Error
        NO_TRANSPORT_SPECIFIED,
        ADD_MESSAGE_HANDLER_FAIL,
        ADD_TIMER_FAIL,
        MESSAGE_VERIFY_FAIL,
        // Info
        INITIALIZED,
        PEER_UPDATES_SENT,
        // Trace
        PEER_UPDATES_RECEIVED,
        STATE_UPDATES_RECEIVED,
    };

    struct Config {
        uint16_t peer_update_interval = 1000;
        PeerManager::Config peer_manager;
        std::shared_ptr<Transport> transport;
        std::shared_ptr<Logger> logger;
    };

    struct Stats {
        uint32_t peer_updates_received;
        uint32_t state_updates_received;
        uint32_t message_verify_failures;
    };

    MeshNode(Config config);

    void sendPeerUpdates();

    PeerManager& getPeerManager() { return _peer_manager; }
    Transport& getTransport() { return *_transport; }
    Logger& getLogger() { return *_logger; }

    const Stats& getStats() const { return _stats; }

private:
    void receiveMessageHandler(const void* buffer, size_t len);

    Stats _stats;
    uint32_t _current_time;
    PeerManager _peer_manager;
    std::shared_ptr<Transport> _transport;
    std::shared_ptr<Logger> _logger;
    flatbuffers::FlatBufferBuilder _fbb;
    std::vector<std::string> _recipients_buffer;
    std::vector<std::string> _connected_peers;
};

}  // namespace vsm
