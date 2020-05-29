#include <vsm/peer_manager.hpp>
#include <algorithm>
#include <cmath>

namespace vsm {

PeerManager::PeerManager(Config config, std::shared_ptr<Logger> logger)
        : _config(std::move(config))
        , _logger(std::move(logger)) {
    if (_config.address.empty()) {
        Error error("Address config empty.", ADDRESS_CONFIG_EMPTY);
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

    IF_PTR(_logger, log, Logger::INFO, Error("Peer manager initialized.", INITIALIZED));
}

PeerManager::ErrorType PeerManager::latchPeer(const char* address, msecs latch_until) {
    if (!address) {
        Error error("Peer address missing.", PEER_ADDRESS_MISSING);
        IF_PTR(_logger, log, Logger::ERROR, error, address);
        return PEER_ADDRESS_MISSING;
    }
    if (_node_info.address == address) {
        Error error("Cannot latch self address.", PEER_IS_SELF);
        IF_PTR(_logger, log, Logger::ERROR, error, address);
        return PEER_IS_SELF;
    }
    auto& peer = _peers[address];
    if (peer.node_info.address.empty()) {
        peer.node_info.address = address;
        _peer_rankings.emplace_back(&peer);
    }
    if (_logger && ((latch_until - peer.latch_until) > _config.latch_duration)) {
        _logger->log(Logger::INFO, Error("Peer latched.", PEER_LATCHED), address);
    }
    peer.latch_until = latch_until;
    return SUCCESS;
}

PeerManager::ErrorType PeerManager::updatePeer(const NodeInfo* node_info) {
    // null check
    if (!node_info) {
        IF_PTR(_logger, log, Logger::WARN, Error("Peer is null.", PEER_IS_NULL), node_info);
        return PEER_IS_NULL;
    }
    // reject missing address
    if (!node_info->address()) {
        IF_PTR(_logger, log, Logger::WARN, Error("Peer address missing.", PEER_ADDRESS_MISSING),
                node_info);
        return PEER_ADDRESS_MISSING;
    }
    // reject updates corresponds to this node
    auto peer_address = node_info->address()->c_str();
    if (_node_info.address == peer_address) {
        return PEER_IS_SELF;
    }
    // reject missing coordinates
    if (!node_info->coordinates()) {
        IF_PTR(_logger, log, Logger::WARN,
                Error("Peer coordinates missing.", PEER_COORDINATES_MISSING), node_info);
        return PEER_COORDINATES_MISSING;
    }
    // check if peer exists in lookup
    auto emplace_result = _peers.emplace(peer_address, Peer{});
    auto& peer = emplace_result.first->second;
    if (emplace_result.second) {
        IF_PTR(_logger, log, Logger::INFO, Error("New peer discovered.", NEW_PEER_DISCOVERED),
                node_info);
        _peer_rankings.emplace_back(&peer);
    } else {
        if (node_info->timestamp() < peer.node_info.timestamp) {
            IF_PTR(_logger, log, Logger::WARN,
                    Error("Peer timestamp is stale.", PEER_TIMESTAMP_STALE), node_info);
            return PEER_TIMESTAMP_STALE;
        }
    }
    node_info->UnPackTo(&(peer.node_info));
    IF_PTR(_logger, log, Logger::TRACE, Error("Peer updated.", PEER_UPDATED), &peer.node_info,
            sizeof(NodeInfoT));
    return SUCCESS;
}

void PeerManager::receivePeerUpdates(const Message* msg, msecs current_time) {
    for (auto node_info : *msg->peers()) {
        if (updatePeer(node_info) == PEER_IS_SELF && msg->source()->address()) {
            latchPeer(msg->source()->address()->c_str(), current_time + _config.latch_duration);
        }
    }
}

std::vector<fb::Offset<NodeInfo>> PeerManager::updatePeerRankings(
        fb::FlatBufferBuilder& fbb, std::vector<std::string>& recipients, msecs current_time) {
    // compute rank costs
    for (auto& peer : _peers) {
        peer.second.rank_cost =
                distanceSqr(peer.second.node_info.coordinates.get(), _node_info.coordinates.get());
        int32_t elapsed_time = current_time.count() - peer.second.node_info.timestamp;
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
    auto latched_end =
            std::lower_bound(_peer_rankings.begin(), _peer_rankings.end(), &latched_div, comp);
    // build ranked peers vector
    std::vector<fb::Offset<NodeInfo>> ranked_peers;
    auto latched_peer = _peer_rankings.begin();
    auto ranked_end = latched_end;
    while (!(latched_peer == latched_end && ranked_end == _peer_rankings.end()) &&
            ranked_peers.size() < _config.connection_degree) {
        if (latched_peer == latched_end) {
            ranked_peers.emplace_back(NodeInfo::Pack(fbb, &((*ranked_end++)->node_info)));
        } else if (ranked_end == _peer_rankings.end()) {
            ranked_peers.emplace_back(NodeInfo::Pack(fbb, &((*latched_peer++)->node_info)));
        } else if ((*latched_peer)->rank_cost > (*ranked_end)->rank_cost) {
            ranked_peers.emplace_back(NodeInfo::Pack(fbb, &((*ranked_end++)->node_info)));
        } else {
            ranked_peers.emplace_back(NodeInfo::Pack(fbb, &((*latched_peer++)->node_info)));
        }
    }
    // saved end indices
    _latched_end = std::distance(_peer_rankings.begin(), latched_end);
    _ranked_end = std::distance(_peer_rankings.begin(), ranked_end);
    // build recipients vector
    recipients.clear();
    for (auto recipient = _peer_rankings.begin(); recipient != ranked_end; ++recipient) {
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

void PeerManager::getRankedPeers(std::vector<const Peer*>& ranked_peers) const {
    ranked_peers.assign(_peer_rankings.begin(), _peer_rankings.begin() + _ranked_end);
    size_t ranked_size = std::min(ranked_peers.size(), _config.connection_degree);
    std::partial_sort(ranked_peers.begin(), ranked_peers.begin() + ranked_size, ranked_peers.end(),
            [](const Peer* a, const Peer* b) { return a->rank_cost < b->rank_cost; });
    ranked_peers.resize(ranked_size);
}

}  // namespace vsm
