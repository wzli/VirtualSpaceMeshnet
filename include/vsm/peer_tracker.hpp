#pragma once
#include <vsm/logger.hpp>
#include <vsm/msg_types_generated.h>

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

        size_t connection_degree = 10;
        size_t lookup_size = 128;
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
    Logger* getLogger() { return _logger.get(); }

private:
    Config _config;
    NodeInfoT _node_info;
    PeerLookup _peers;
    std::vector<Peer*> _peer_rankings;
    std::vector<std::string> _recipients;
    std::shared_ptr<Logger> _logger;
};

}  // namespace vsm
