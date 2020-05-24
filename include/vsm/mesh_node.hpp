#pragma once

#include <vsm/transport.hpp>
#include <vsm/peer_manager.hpp>

namespace vsm {

class MeshNode {
public:
    struct Config {
        uint16_t beacon_interval = 1000;
    };

    MeshNode(const Config& config, std::unique_ptr<Transport> transport);

    void addPeer(const std::string& peer_address);

    const Transport& getTransport() const { return *_transport; }
    const PeerManager& getPeerManager() const { return _peer_manager; }

private:
    Config _config;
    std::unique_ptr<Transport> _transport;
    PeerManager _peer_manager;
};

}  // namespace vsm
