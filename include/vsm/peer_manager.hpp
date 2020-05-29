#pragma once
#include <vsm/logger.hpp>
#include <vsm/msg_types_generated.h>

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace vsm {

namespace fb = flatbuffers;

inline float distanceSqr(const Vec2* a, const Vec2* b) {
    if (!a || !b) {
        return std::numeric_limits<float>::max();
    }
    float dx = b->x() - a->x();
    float dy = b->y() - a->y();
    return (dx * dx) + (dy * dy);
}

struct Peer {
    NodeInfoT node_info;
    msecs latch_until = msecs(0);
    float rank_cost = 0;
};

class PeerManager {
public:
    using PeerLookup = std::unordered_map<std::string, Peer>;

    struct PeerRange {
        std::vector<Peer*>::const_iterator begin;
        std::vector<Peer*>::const_iterator end;
    };

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
        PEER_TIMESTAMP_STALE,
        // Info
        INITIALIZED,
        NEW_PEER_DISCOVERED,
        PEER_LATCHED,
        // Trace
        PEER_UPDATED,
        PEER_IS_SELF,
        PEER_RANKINGS_GENERATED,
        PEER_LOOKUP_TRUNCATED,
    };

    struct Config {
        std::string name;
        std::string address;
        Vec2 coordinates;

        msecs latch_duration = msecs(1000);
        size_t connection_degree = 10;
        size_t lookup_size = 128;
        float rank_decay = 0.00001f;
    };

    PeerManager(Config config, std::shared_ptr<Logger> logger = nullptr);

    ErrorType latchPeer(const char* address, msecs latch_until);

    ErrorType updatePeer(const NodeInfo* node_info);

    void receivePeerUpdates(const Message* msg, msecs current_time);

    std::vector<fb::Offset<NodeInfo>> updatePeerRankings(
            fb::FlatBufferBuilder& fbb, std::vector<std::string>& recipients, msecs current_time);

    const PeerLookup& getPeers() const { return _peers; }

    PeerRange getRecipientPeers() const {
        return {_peer_rankings.begin(), _peer_rankings.begin() + _ranked_end};
    }
    PeerRange getLatchedPeers() const {
        return {_peer_rankings.begin(), _peer_rankings.begin() + _latched_end};
    }
    void getRankedPeers(std::vector<const Peer*>& ranked_peers) const;

    NodeInfoT& getNodeInfo() { return _node_info; }
    Logger* getLogger() { return _logger.get(); }

private:
    Config _config;
    NodeInfoT _node_info;
    PeerLookup _peers;
    std::vector<Peer*> _peer_rankings;
    std::shared_ptr<Logger> _logger;
    size_t _latched_end;
    size_t _ranked_end;
};

}  // namespace vsm
