#pragma once

#include <vsm/transport.hpp>
#include <vsm/peer_manager.hpp>

namespace vsm {

class MeshNode {
public:
    struct Config {
        uint16_t beacon_interval = 1000;
    };

    MeshNode(std::unique_ptr<Transport> transport);

    int init(const Config& config);

    Transport& getTransport() { return *_transport; }
    PeerManager& getPeerManager() { return _peer_manager; }

    static const Message* getMessage(const void* buffer, size_t len);

private:
    void recvPeerUpdates(const void* buffer, size_t len);
    void recvStateUpdates(const void* buffer, size_t len);

    std::unique_ptr<Transport> _transport;
    PeerManager _peer_manager;
};

}  // namespace vsm
