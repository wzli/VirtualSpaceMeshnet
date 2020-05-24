#pragma once
#include <vsm/msg_types_generated.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace vsm {

struct Peer {
    NodeInfoT node_info;
    uint32_t last_contact;
};

class PeerManager {
public:
    PeerManager();

    void contactPeer(const std::string& address, uint32_t expiry);
    bool updatePeer(const NodeInfo* node_info);

    void generateBeacon();

    const std::unordered_map<std::string, Peer>& getPeers() const { return _peers; }

private:
    std::unordered_map<std::string, Peer> _peers;
    std::vector<Peer*> _peer_rankings;
};

}  // namespace vsm
