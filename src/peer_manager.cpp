#include <vsm/peer_manager.hpp>
#include <algorithm>
#include <cmath>

namespace vsm {

PeerManager::PeerManager(Config config)
        : _config(std::move(config))
        , _logger(std::move(_config.logger)) {
    if (_config.name.empty()) {
        Error error("Empty address.", EMPTY_ADDRESS);
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    if (_config.rank_decay < 0) {
        Error error("Negative rank decay.", NEGATIVE_RANK_DECAY);
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    _node_info.name = std::move(_config.name);
    _node_info.address = std::move(_config.address);
    _node_info.coordinates = std::make_unique<Vec2>(std::move(_config.coordinates));

    Error error = Error("Peer manager initialized.", INITIALIZED);
    IF_PTR(_logger, log, Logger::INFO, error);
}

void PeerManager::latchPeer(std::string address, uint32_t latch_until) {
    auto& peer = _peers[address];
    peer.node_info.address = std::move(address);
    peer.latch_until = latch_until;
}

bool PeerManager::updatePeer(const NodeInfo* node_info) {
    // null check
    if (!node_info || !node_info->address()) {
        IF_PTR(_logger, log, Logger::WARN, Error("Peer address missing.", PEER_ADDRESS_MISSING),
                node_info);
        return false;
    }
    // reject updates corresponds to this node
    auto peer_address = node_info->address()->c_str();
    if (_node_info.address == peer_address) {
        return false;
    }
    // check if peer exists in lookup
    auto emplace_result = _peers.emplace(peer_address, Peer{});
    auto& peer = emplace_result.first->second;
    if (emplace_result.second) {
        IF_PTR(_logger, log, Logger::INFO, Error("New Peer discovered.", NEW_PEER_DISCOVERED),
                node_info);
        _peer_rankings.emplace_back(&peer);
    } else {
        if (node_info->timestamp() < peer.node_info.timestamp) {
            IF_PTR(_logger, log, Logger::WARN,
                    Error("Peer timestamp is stale.", PEER_TIMESTAMP_STALE), node_info);
            return false;
        }
        IF_PTR(_logger, log, Logger::TRACE, Error("Peer updated.", PEER_UPDATED), &peer.node_info,
                sizeof(NodeInfoT));
    }
    node_info->UnPackTo(&(peer.node_info));
    return true;
}

void PeerManager::recvPeerUpdates(const fb::Vector<fb::Offset<NodeInfo>>* peer_updates) {
    for (auto node_info : *peer_updates) {
        updatePeer(node_info);
    }
}

std::vector<fb::Offset<NodeInfo>> PeerManager::updatePeerRankings(
        fb::FlatBufferBuilder& fbb, std::vector<std::string>& recipients, uint32_t current_time) {
    // compute rank costs
    for (auto& peer : _peers) {
        peer.second.rank_cost =
                distanceSqr(*(peer.second.node_info.coordinates), *(_node_info.coordinates));
        int32_t elapsed_time = current_time - peer.second.node_info.timestamp;
        if (elapsed_time > 0) {
            peer.second.rank_cost *= std::exp(_config.rank_decay * elapsed_time);
        }
    }
    // prioritized latched peers
    auto comp = [this, current_time](const Peer* a, const Peer* b) {
        bool a_latched = a->latch_until > current_time;
        bool b_latched = b->latch_until > current_time;
        return (a_latched > b_latched) ||
               ((a_latched == b_latched) && (a->rank_cost < b->rank_cost));
    };
    // compute ranking
    std::sort(_peer_rankings.begin(), _peer_rankings.end(), comp);
    // find the boundary between latched and non-latched peers
    const Peer latched_div{{}, current_time, 0};
    const auto latched_end =
            std::lower_bound(_peer_rankings.begin(), _peer_rankings.end(), &latched_div, comp);
    // build ranked peers vector
    std::vector<fb::Offset<NodeInfo>> ranked_peers;
    auto latched_peer = _peer_rankings.begin();
    auto ranked_peer = latched_end;
    while (!(latched_peer == latched_end && ranked_peer == _peer_rankings.end()) &&
            ranked_peers.size() < _config.connection_degree) {
        if (latched_peer == latched_end) {
            ranked_peers.emplace_back(NodeInfo::Pack(fbb, &((*ranked_peer++)->node_info)));
        } else if (ranked_peer == _peer_rankings.end()) {
            ranked_peers.emplace_back(NodeInfo::Pack(fbb, &((*latched_peer++)->node_info)));
        } else if ((*latched_peer)->rank_cost > (*ranked_peer)->rank_cost) {
            ranked_peers.emplace_back(NodeInfo::Pack(fbb, &((*ranked_peer++)->node_info)));
        } else {
            ranked_peers.emplace_back(NodeInfo::Pack(fbb, &((*latched_peer++)->node_info)));
        }
    }
    // build recipients vector
    recipients.clear();
    for (auto recipient = _peer_rankings.begin(); recipient != ranked_peer; ++recipient) {
        recipients.emplace_back((*recipient)->node_info.address);
    }
    IF_PTR(_logger, log, Logger::TRACE, Error("Peer rankings generated.", PEER_RANKINGS_GENERATED));
    // remove lowest ranked peers
    if (_peers.size() > _config.lookup_size) {
        for (auto low_rank_peer = _peer_rankings.begin() + _config.lookup_size;
                low_rank_peer != _peer_rankings.end(); ++low_rank_peer) {
            _peers.erase((*low_rank_peer)->node_info.address);
        }
        _peer_rankings.resize(_config.lookup_size);
        IF_PTR(_logger, log, Logger::TRACE, Error("Peer lookup truncated.", PEER_LOOKUP_TRUNCATED));
    }
    return ranked_peers;
}

}  // namespace vsm
