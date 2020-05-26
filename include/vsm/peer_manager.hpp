#pragma once
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
    PeerManager(std::string name, Vector2 coordinates);

    void latchPeer(std::string address, uint32_t latch_until);
    bool updatePeer(const NodeInfo* node_info);

    void generateBeacon();

    const std::unordered_map<std::string, Peer>& getPeers() const { return _peers; }

    NodeInfoT& editNodeInfo() { return _node_info; }

private:
    NodeInfoT _node_info;
    std::unordered_map<std::string, Peer> _peers;
    std::vector<Peer*> _peer_rankings;
};

}  // namespace vsm
