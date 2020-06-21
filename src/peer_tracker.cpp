#include <vsm/peer_tracker.hpp>
#include <algorithm>

namespace vsm {

PeerTracker::PeerTracker(Config config, std::shared_ptr<Logger> logger)
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
    _node_info.coordinates = std::move(_config.coordinates);

    IF_PTR(_logger, log, Logger::INFO, Error("Peer tracker initialized.", INITIALIZED));
}

PeerTracker::ErrorType PeerTracker::latchPeer(const char* address, float rank_factor) {
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
    peer.rank_factor = std::min(rank_factor, peer.rank_factor);
    IF_PTR(_logger, log, Logger::INFO, Error("Peer latched.", PEER_LATCHED), address);
    return SUCCESS;
}

PeerTracker::ErrorType PeerTracker::updatePeer(const NodeInfo* node_info, bool is_source) {
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
    } else if (is_source) {
        if (node_info->sequence() <= peer.source_sequence) {
            IF_PTR(_logger, log, Logger::DEBUG,
                    Error("Source sequence is stale.", SOURCE_SEQUENCE_STALE), node_info);
            return SOURCE_SEQUENCE_STALE;
        }
        peer.source_sequence = node_info->sequence();
    } else if (node_info->sequence() <= peer.node_info.sequence) {
        IF_PTR(_logger, log, Logger::DEBUG, Error("Peer sequence is stale.", PEER_SEQUENCE_STALE),
                node_info);
        return PEER_SEQUENCE_STALE;
    }
    node_info->UnPackTo(&(peer.node_info));
    peer.rank_factor = std::min(peer.rank_factor, 1.0f);
    IF_PTR(_logger, log, Logger::TRACE, Error("Peer updated.", PEER_UPDATED), &peer.node_info,
            sizeof(NodeInfoT));
    return SUCCESS;
}

int PeerTracker::receivePeerUpdates(const Message* msg) {
    if (!msg || !msg->peers()) {
        return 0;
    }
    int peers_updated = 0;
    for (auto node_info : *msg->peers()) {
        auto update_error = updatePeer(node_info);
        if (update_error == PEER_IS_SELF) {
            // catch up to previous known sequence number
            _node_info.sequence = std::max(_node_info.sequence, node_info->sequence());
            // respond to peers than ranked this node
            if (msg->source() && msg->source()->address()) {
                _recipients.emplace_back(msg->source()->address()->c_str());
            }
        }
        peers_updated += update_error == SUCCESS;
    }
    return peers_updated;
}

std::vector<fb::Offset<NodeInfo>> PeerTracker::updatePeerRankings(
        fb::FlatBufferBuilder& fbb, std::vector<std::string>& recipients) {
    // compute rank costs
    for (auto& peer : _peers) {
        peer.second.rank_cost = peer.second.radialCost(_node_info.coordinates);
        peer.second.rank_factor *= 1.0f + _config.rank_decay;
    }
    // sort rankings
    std::sort(_peer_rankings.begin(), _peer_rankings.end(),
            [](const Peer* a, const Peer* b) { return a->rank_cost < b->rank_cost; });
    // create ranked peers list
    std::vector<fb::Offset<NodeInfo>> ranked_peers;
    for (size_t i = 0; i < _config.connection_degree && i < _peer_rankings.size(); ++i) {
        ranked_peers.emplace_back(NodeInfo::Pack(fbb, &_peer_rankings[i]->node_info));
        _recipients.emplace_back(_peer_rankings[i]->node_info.address);
    }
    // remove duplicates from recipients list
    std::sort(_recipients.begin(), _recipients.end());
    _recipients.erase(std::unique(_recipients.begin(), _recipients.end()), _recipients.end());
    // swap recipients list with output and clear;
    _recipients.swap(recipients);
    _recipients.clear();

    IF_PTR(_logger, log, Logger::TRACE, Error("Peer rankings generated.", PEER_RANKINGS_GENERATED));
    // remove lowest ranked peers
    if (_peers.size() > _config.lookup_size) {
        for (size_t i = _config.lookup_size; i < _peer_rankings.size(); ++i) {
            _peers.erase(_peer_rankings[i]->node_info.address);
        }
        _peer_rankings.resize(_config.lookup_size);
        IF_PTR(_logger, log, Logger::TRACE, Error("Peer lookup truncated.", PEER_LOOKUP_TRUNCATED));
    }
    // tick node sequence
    ++_node_info.sequence;
    return ranked_peers;
}

}  // namespace vsm
