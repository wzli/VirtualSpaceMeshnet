#include <vsm/peer_manager.hpp>

namespace vsm {

PeerManager::PeerManager(Config config)
        : _logger(std::move(config.logger)) {
    _node_info.name = std::move(config.name);
    _node_info.address = std::move(config.address);
    _node_info.coordinates = std::make_unique<Vector2>(std::move(config.coordinates));
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
        _logger->log(Logger::TRACE, Error("Peer updated.", PEER_UPDATED), &peer.node_info,
                sizeof(NodeInfoT));
    }
    return true;
}

void PeerManager::generateBeacon() {
    if (_logger) {
        _logger->log(Logger::TRACE, Error("Beacon generated.", BEACON_GENERATED));
    }
}

}  // namespace vsm
