#pragma once
#include <vsm/logger.hpp>
#include <vsm/msg_types_generated.h>

#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace vsm {

namespace fb = flatbuffers;

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

struct Peer {
    NodeInfoT node_info;
    uint32_t source_sequence = 0;
    float rank_factor = 1;
    float rank_cost = 0;

    template <class Vec>
    float radialCost(const Vec& from) const {
        return (distanceSqr(from, node_info.coordinates) * rank_factor) -
               std::copysign(
                       node_info.power_radius * node_info.power_radius, node_info.power_radius);
    }
};

class PeerTracker {
public:
    using PeerLookup = std::unordered_map<std::string, Peer>;

    enum ErrorType {
        SUCCESS = 0,
        START_OFFSET = 200,
        // Error
        ADDRESS_CONFIG_EMPTY,
        NEGATIVE_RANK_DECAY,
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
        PEER_RANKINGS_GENERATED,
        PEER_LOOKUP_TRUNCATED,
    };

    struct Config {
        std::string name;
        std::string address;
        std::vector<float> coordinates;

        float power_radius = 0;
        size_t connection_degree = 10;
        size_t lookup_size = 128;  // 0 is inf
        float rank_decay = 0.0f;
    };

    PeerTracker(Config config, std::shared_ptr<Logger> logger = nullptr);

    ErrorType latchPeer(const char* address, float rank_factor = 0);

    ErrorType updatePeer(const NodeInfo* node_info, bool is_source = false);

    int receivePeerUpdates(const Message* msg);

    std::vector<fb::Offset<NodeInfo>> updatePeerRankings(
            fb::FlatBufferBuilder& fbb, std::vector<std::string>& recipients);

    const PeerLookup& getPeers() const { return _peers; }
    const std::vector<Peer*> getPeerRankings() const { return _peer_rankings; }

    NodeInfoT& getNodeInfo() { return _node_info; }
    const NodeInfoT& getNodeInfo() const { return _node_info; }

    Logger* getLogger() { return _logger.get(); }
    const Logger* getLogger() const { return _logger.get(); }

    template <class Vec>
    const Peer& nearestPeer(const Vec& coordinates, const std::vector<std::string>& peers) const {
        const Peer* min_radial_peer = &_peers.at(_node_info.address);
        float min_radial_cost = min_radial_peer->radialCost(coordinates);
        for (const auto& peer_address : peers) {
            auto peer = _peers.find(peer_address);
            if (peer == _peers.end()) {
                continue;
            }
            float radial_cost = peer->second.radialCost(coordinates);
            if (radial_cost < min_radial_cost) {
                min_radial_cost = radial_cost;
                min_radial_peer = &peer->second;
            }
        }
        return *min_radial_peer;
    }

private:
    Config _config;
    PeerLookup _peers;
    NodeInfoT& _node_info;
    std::vector<Peer*> _peer_rankings;
    std::vector<std::string> _recipients;
    std::shared_ptr<Logger> _logger;
};

}  // namespace vsm
