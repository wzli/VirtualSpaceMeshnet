#pragma once
#include <vsm/logger.hpp>
#include <vsm/msg_types_generated.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace vsm {

static float distanceSqr(const Vector2& a, const Vector2& b) {
    float dx = b.x() - a.x();
    float dy = b.y() - a.y();
    return (dx * dx) + (dy * dy);
}

struct Peer {
    NodeInfoT node_info;
    uint32_t latch_until;
};

class PeerManager {
public:
    using PeerLookup = std::unordered_map<std::string, Peer>;

    enum ErrorType {
        START_OFFSET = 200,
        // Warn
        PEER_ADDRESS_MISSING,
        PEER_TIMESTAMP_STALE,
        // Info
        NEW_PEER_DISCOVERED,
        // Trace
        PEER_UPDATED,
        PEER_UPDATES_GENERATED,
    };

    struct Config {
        std::string name;
        std::string address;
        Vector2 coordinates;
        std::shared_ptr<Logger> logger;
        uint16_t connection_degree = 10;
        uint16_t peer_lookup_size = 128;
    };

    PeerManager(Config config);

    void latchPeer(std::string address, uint32_t latch_until);
    bool updatePeer(const NodeInfo* node_info, size_t buf_size);

    void generateBeacon();

    void updatePeerRankings(uint32_t current_time);

    const PeerLookup& getPeers() const { return _peers; }

    NodeInfoT& getNodeInfo() { return _node_info; }

private:
    std::shared_ptr<Logger> _logger;
    NodeInfoT _node_info;
    PeerLookup _peers;
    std::vector<Peer*> _peer_rankings;
};

}  // namespace vsm
