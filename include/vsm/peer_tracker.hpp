#pragma once
#include <vsm/logger.hpp>
#include <vsm/msg_types_generated.h>

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace vsm {

template <class VecA, class VecB>
float distanceSqr(const VecA& a, const VecB& b) {
    if (a.size() != b.size()) {
        return std::numeric_limits<float>::max();
    }
    float d2 = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        float d = b[i] - a[i];
        d2 += d * d;
    }
    return d2;
}

static inline uint32_t add32(uint32_t a, uint32_t b) {
    return (a > 0xFFFFFFFF - b) ? 0xFFFFFFFF : a + b;
}

struct Peer {
    NodeInfoT node_info;
    uint32_t source_sequence = 0;
    uint32_t latch_until = 0;
    uint32_t track_until = 0;
};

class PeerTracker {
public:
    using PeerLookup = std::unordered_map<std::string, Peer>;

    enum ErrorType {
        SUCCESS = 0,
        START_OFFSET = 200,
        // Error
        ADDRESS_CONFIG_EMPTY,
        // Warn
        PEER_IS_NULL,
        PEER_ADDRESS_MISSING,
        PEER_COORDINATES_MISSING,
        // Info
        INITIALIZED,
        NEW_PEER_DISCOVERED,
        PEER_LATCHED,
        // Debug
        PEER_SEQUENCE_STALE,
        SOURCE_SEQUENCE_STALE,
        // Trace
        PEER_UPDATED,
        PEER_IS_SELF,
        PEER_SELECTIONS_GENERATED,
    };

    struct Config {
        std::string name;
        std::string address;
        std::vector<float> coordinates;
        uint32_t group_mask = 0xFFFFFFFF;
        uint32_t tracking_duration = 0xFFFFFFFF;
    };

    PeerTracker(Config config, std::shared_ptr<Logger> logger = nullptr);

    ErrorType latchPeer(const char* address, uint32_t latch_duration = 0xFFFFFFFF);

    ErrorType updatePeer(const NodeInfo* node_info, bool is_source = false);

    int receivePeerUpdates(const Message* msg);

    void updatePeerSelections(
            std::vector<std::string>& selected_peers, std::vector<std::string>& recipients);

    // accessors (FYI they are not thread safe)
    const PeerLookup& getPeers() const { return _peers; }

    NodeInfoT& getNodeInfo() { return _node_info; }
    const NodeInfoT& getNodeInfo() const { return _node_info; }

    Logger* getLogger() { return _logger.get(); }
    const Logger* getLogger() const { return _logger.get(); }

    template <class Vec>
    const NodeInfoT& nearestPeer(
            const Vec& coordinates, const std::vector<std::string>& peers) const {
        const auto* nearest_node = &_node_info;
        float min_distance_sqr = distanceSqr(coordinates, nearest_node->coordinates);
        for (const auto& peer_address : peers) {
            auto peer = _peers.find(peer_address);
            if (peer == _peers.end()) {
                continue;
            }
            float distance_sqr = distanceSqr(coordinates, peer->second.node_info.coordinates);
            if (distance_sqr < min_distance_sqr) {
                min_distance_sqr = distance_sqr;
                nearest_node = &peer->second.node_info;
            }
        }
        return *nearest_node;
    }

private:
    Config _config;
    PeerLookup _peers;
    NodeInfoT _node_info;
    std::vector<std::string> _recipients;
    std::shared_ptr<Logger> _logger;
};

}  // namespace vsm
