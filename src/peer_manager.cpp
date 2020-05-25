#include <vsm/peer_manager.hpp>

namespace vsm {

PeerManager::PeerManager() {}

void PeerManager::latchPeer(const std::string& address, uint32_t latch_until) {
    auto& peer = _peers[address];
    peer.node_info.address = address;
    peer.latch_until = latch_until;
}

bool PeerManager::updatePeer(const NodeInfo* node_info) {
    if (!node_info || !node_info->address()) {
        return false;
    }
    auto& peer = _peers[node_info->address()->str()];
    if (node_info->timestamp() < peer.node_info.timestamp) {
        return false;
    }
    node_info->UnPackTo(&(peer.node_info));
    return true;
}

void PeerManager::generateBeacon() {}

}  // namespace vsm
