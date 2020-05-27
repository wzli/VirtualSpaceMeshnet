#include <vsm/peer_manager.hpp>
#include <cmath>

namespace vsm {

PeerManager::PeerManager(Config config)
        : _config(std::move(config))
        , _logger(std::move(_config.logger)) {
    if (_config.rank_decay < 0) {
        Error error("Negative rank decay.", NEGATIVE_RANK_DECAY);
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    _node_info.name = std::move(_config.name);
    _node_info.address = std::move(_config.address);
    _node_info.coordinates = std::make_unique<Vector2>(std::move(_config.coordinates));

    Error error = Error("Peer manager initialized.", INITIALIZED);
    IF_PTR(_logger, log, Logger::INFO, error);
}

void PeerManager::latchPeer(std::string address, uint32_t latch_until) {
    auto& peer = _peers[address];
    peer.node_info.address = std::move(address);
    peer.latch_until = latch_until;
}

bool PeerManager::updatePeer(const NodeInfo* node_info, size_t buf_size) {
    if (!node_info || !node_info->address()) {
        IF_PTR(_logger, log, Logger::WARN, Error("Peer address missing.", PEER_ADDRESS_MISSING),
                node_info, buf_size);
        return false;
    }
    auto emplace_result = _peers.emplace(node_info->address()->str(), Peer{});
    auto& peer = emplace_result.first->second;
    if (emplace_result.second) {
        IF_PTR(_logger, log, Logger::INFO, Error("New Peer discovered.", NEW_PEER_DISCOVERED),
                node_info, buf_size);
        _peer_rankings.emplace_back(&peer);
    } else {
        if (node_info->timestamp() < peer.node_info.timestamp) {
            IF_PTR(_logger, log, Logger::WARN,
                    Error("Peer timestamp is stale.", PEER_TIMESTAMP_STALE), node_info, buf_size);
            return false;
        }
        IF_PTR(_logger, log, Logger::TRACE, Error("Peer updated.", PEER_UPDATED), &peer.node_info,
                sizeof(NodeInfoT));
    }
    node_info->UnPackTo(&(peer.node_info));
    return true;
}

void PeerManager::generateBeacon() {
    IF_PTR(_logger, log, Logger::TRACE, Error("Peer updates generated.", PEER_UPDATES_GENERATED));
}

void PeerManager::updatePeerRankings(uint32_t current_time) {
    for (auto& peer : _peers) {
        peer.second.rank_cost =
                distanceSqr(*(peer.second.node_info.coordinates), *(_node_info.coordinates)) *
                std::exp(_config.rank_decay * (current_time - peer.second.node_info.timestamp));
    }
    auto comp = [this, current_time](const Peer* a, const Peer* b) {
        bool a_latched = a->latch_until >= current_time;
        bool b_latched = b->latch_until >= current_time;
        return (a_latched > b_latched) ||
               ((a_latched == b_latched) && (a->rank_cost < b->rank_cost));
    };
    std::sort(_peer_rankings.begin(), _peer_rankings.end(), comp);
    for (auto& ranking : _peer_rankings) {
        printf("name %s x %f latch %d\n", ranking->node_info.name.c_str(),
                ranking->node_info.coordinates->x(), ranking->latch_until);
    }
}

}  // namespace vsm
