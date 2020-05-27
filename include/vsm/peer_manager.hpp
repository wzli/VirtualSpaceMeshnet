#pragma once
#include <vsm/logger.hpp>
#include <vsm/msg_types_generated.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace vsm {

struct Peer {
    NodeInfoT node_info;
    uint32_t latch_until;
};

class PeerManager {
public:
    enum ErrorType {
        START_OFFSET = 200,
        // Warn
        PEER_ADDRESS_MISSING,
        PEER_TIMESTAMP_STALE,
        // Trace
        PEER_UPDATED,
        BEACON_GENERATED,
    };

    struct Config {
        std::string name;
        std::string address;
        Vector2 coordinates;
        std::shared_ptr<Logger> logger;
    };

    PeerManager(Config config);

    void latchPeer(std::string address, uint32_t latch_until);
    bool updatePeer(const NodeInfo* node_info, size_t buf_size);

    void generateBeacon();

    const std::unordered_map<std::string, Peer>& getPeers() const { return _peers; }

    NodeInfoT& getNodeInfo() { return _node_info; }

private:
    std::shared_ptr<Logger> _logger;
    NodeInfoT _node_info;
    std::unordered_map<std::string, Peer> _peers;
    std::vector<Peer*> _peer_rankings;
};

}  // namespace vsm
