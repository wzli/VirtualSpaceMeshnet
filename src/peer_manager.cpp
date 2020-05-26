#include <vsm/peer_manager.hpp>

namespace vsm {

PeerManager::PeerManager(std::string name, Vector2 coordinates, std::shared_ptr<Logger> logger)
        : _logger(std::move(logger)) {
    _node_info.name = std::move(name);
    _node_info.coordinates = std::unique_ptr<Vector2>(new Vector2(std::move(coordinates)));
}

void PeerManager::latchPeer(std::string address, uint32_t latch_until) {
    auto& peer = _peers[address];
    peer.node_info.address = std::move(address);
    peer.latch_until = latch_until;
}

bool PeerManager::updatePeer(const NodeInfo* node_info, size_t buf_size) {
    if (!node_info || !node_info->address()) {
        if (_logger) {
            _logger->log(Logger::WARN, Error("Peer address missing.", PEER_ADDRESS_MISSING),
                    node_info, buf_size);
        }
        return false;
    }
    auto& peer = _peers[node_info->address()->str()];
    if (node_info->timestamp() < peer.node_info.timestamp) {
        if (_logger) {
            _logger->log(Logger::TRACE, Error("Peer timestamp is stale.", PEER_TIMESTAMP_STALE),
                    node_info, buf_size);
        }
        return false;
    }
    node_info->UnPackTo(&(peer.node_info));
    if (_logger) {
        _logger->log(
                Logger::TRACE, Error("Peer updated.", PEER_UPDATED), &node_info, sizeof(NodeInfoT));
    }
    return true;
}

void PeerManager::generateBeacon() {
    if (_logger) {
        _logger->log(Logger::TRACE, Error("Beacon generated.", BEACON_GENERATED));
    }
}

}  // namespace vsm
