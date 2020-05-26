#include <vsm/peer_manager.hpp>

namespace vsm {

PeerManager::PeerManager(std::string name, Vector2 coordinates, Logger* logger)
        : _logger(logger) {
    _node_info.name = std::move(name);
    _node_info.coordinates = std::unique_ptr<Vector2>(new Vector2(std::move(coordinates)));
}

void PeerManager::latchPeer(std::string address, uint32_t latch_until) {
    auto& peer = _peers[address];
    peer.node_info.address = std::move(address);
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

void PeerManager::generateBeacon() {
    _logger ? _logger->log(Logger::TRACE, Error("Beacon Tick")) : void(0);
}

}  // namespace vsm
