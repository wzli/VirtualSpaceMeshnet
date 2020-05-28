#pragma once
#include <vsm/logger.hpp>
#include <vsm/msg_types_generated.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace vsm {

inline float distanceSqr(const Vector2& a, const Vector2& b) {
    float dx = b.x() - a.x();
    float dy = b.y() - a.y();
    return (dx * dx) + (dy * dy);
}

struct Peer {
    NodeInfoT node_info;
    uint32_t latch_until;
    float rank_cost;
};

class PeerManager {
public:
    using PeerLookup = std::unordered_map<std::string, Peer>;
    using PeersVector = std::vector<flatbuffers::Offset<NodeInfo>>;

    enum ErrorType {
        START_OFFSET = 200,
        // Error
        NEGATIVE_RANK_DECAY,
        // Warn
        PEER_ADDRESS_MISSING,
        PEER_TIMESTAMP_STALE,
        // Info
        INITIALIZED,
        NEW_PEER_DISCOVERED,
        // Trace
        PEER_UPDATED,
        PEER_RANKINGS_GENERATED,
        PEER_LOOKUP_TRUNCATED,
    };

    struct Config {
        std::string name;
        std::string address;
        Vector2 coordinates;
        std::shared_ptr<Logger> logger;

        uint16_t connection_degree = 10;
        uint16_t latch_duration = 1000;
        uint16_t lookup_size = 128;
        float rank_decay = 0.001f;
    };

    PeerManager(Config config);

    void latchPeer(std::string address, uint32_t latch_until);
    bool updatePeer(const NodeInfo* node_info, size_t buf_size);

    void generateBeacon();

    PeersVector updatePeerRankings(flatbuffers::FlatBufferBuilder& fbb,
            std::vector<std::string>& recipients, uint32_t current_time);

    const PeerLookup& getPeers() const { return _peers; }

    NodeInfoT& getNodeInfo() { return _node_info; }

private:
    Config _config;
    NodeInfoT _node_info;
    PeerLookup _peers;
    std::vector<Peer*> _peer_rankings;
    std::shared_ptr<Logger> _logger;
};

}  // namespace vsm
