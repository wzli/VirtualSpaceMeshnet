#include <vsm/peer_tracker.hpp>
#include <vsm/quick_hull.hpp>
#include <algorithm>

namespace vsm {

PeerTracker::PeerTracker(Config config, std::shared_ptr<Logger> logger)
        : _config(std::move(config))
        , _logger(std::move(logger)) {
    if (_config.address.empty()) {
        Error error(STRERR(ADDRESS_CONFIG_EMPTY));
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    _node_info.name = std::move(_config.name);
    _node_info.address = std::move(_config.address);
    _node_info.coordinates = std::move(_config.coordinates);
    _node_info.group_mask = std::move(_config.group_mask);

    IF_PTR(_logger, log, Logger::INFO, Error(STRERR(PeerTracker::INITIALIZED)));
}

PeerTracker::ErrorType PeerTracker::latchPeer(const char* address, uint32_t latch_duration) {
    if (!address) {
        Error error(STRERR(PEER_ADDRESS_MISSING));
        IF_PTR(_logger, log, Logger::ERROR, error, address);
        return PEER_ADDRESS_MISSING;
    }
    if (_node_info.address == address) {
        Error error("Cannot latch " STRERR(PEER_IS_SELF));
        IF_PTR(_logger, log, Logger::ERROR, error, address);
        return PEER_IS_SELF;
    }
    auto& peer = _peers[address];
    if (peer.node_info.address.empty()) {
        peer.node_info.address = address;
    }
    peer.latch_until = add32(_node_info.sequence, latch_duration);
    IF_PTR(_logger, log, Logger::INFO, Error(STRERR(PEER_LATCHED)), address);
    return SUCCESS;
}

PeerTracker::ErrorType PeerTracker::updatePeer(const NodeInfo* node_info, bool is_source) {
    // null check
    if (!node_info) {
        IF_PTR(_logger, log, Logger::WARN, Error(STRERR(PEER_IS_NULL)), node_info);
        return PEER_IS_NULL;
    }
    // reject missing address
    if (!node_info->address()) {
        IF_PTR(_logger, log, Logger::WARN, Error(STRERR(PEER_ADDRESS_MISSING)), node_info);
        return PEER_ADDRESS_MISSING;
    }
    // reject updates corresponds to this node
    auto peer_address = node_info->address()->c_str();
    if (_node_info.address == peer_address) {
        return PEER_IS_SELF;
    }
    // reject missing coordinates
    if (!node_info->coordinates()) {
        IF_PTR(_logger, log, Logger::WARN, Error(STRERR(PEER_COORDINATES_MISSING)), node_info);
        return PEER_COORDINATES_MISSING;
    }
    // check if peer exists in lookup
    auto emplace_result = _peers.emplace(peer_address, Peer{});
    auto& peer = emplace_result.first->second;
    if (emplace_result.second) {
        IF_PTR(_logger, log, Logger::INFO, Error(STRERR(NEW_PEER_DISCOVERED)), node_info);
    } else if (is_source) {
        // reset rank factor if any message is directly recieved from source
        peer.track_until = add32(_node_info.sequence, _config.tracking_duration);
        if (node_info->sequence() <= peer.source_sequence) {
            IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(SOURCE_SEQUENCE_STALE)), node_info);
            return SOURCE_SEQUENCE_STALE;
        }
        peer.source_sequence = node_info->sequence();
    } else if (node_info->sequence() <= peer.node_info.sequence) {
        IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(PEER_SEQUENCE_STALE)), node_info);
        return PEER_SEQUENCE_STALE;
    }
    node_info->UnPackTo(&(peer.node_info));
    peer.track_until = add32(_node_info.sequence, _config.tracking_duration);
    IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(PEER_UPDATED)), &peer.node_info,
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
            // respond to peers that contacted this node
            if (msg->source() && msg->source()->address()) {
                _recipients.emplace_back(msg->source()->address()->c_str());
            }
        }
        peers_updated += update_error == SUCCESS;
    }
    return peers_updated;
}

void PeerTracker::updatePeerSelections(
        std::vector<std::string>& selected_peers, std::vector<std::string>& recipients) {
    selected_peers.clear();
    recipients.clear();
    // build candidate points list
    std::vector<const Peer*> candidate_peers;
    std::vector<std::vector<float>> candidate_points;
    candidate_peers.reserve(_peers.size());
    candidate_points.reserve(_peers.size());
    for (auto peer = _peers.begin(); peer != _peers.end();) {
        // add latched peer to selected list
        if (peer->second.latch_until >= _node_info.sequence) {
            selected_peers.emplace_back(peer->second.node_info.address);
            _recipients.emplace_back(peer->second.node_info.address);
            ++peer;
            continue;
        }
        // delete peer if tracking expired
        if (peer->second.track_until < _node_info.sequence) {
            peer = _peers.erase(peer);
            continue;
        }
        // add peer as candidate only of they belong in the same group
        if (_node_info.group_mask & peer->second.node_info.group_mask) {
            candidate_points.emplace_back(peer->second.node_info.coordinates);
            candidate_peers.emplace_back(&peer->second);
        }
        ++peer;
    }
    // add interior hull neighbors to selected peers
    QuickHull::sphereInversion(candidate_points, _node_info.coordinates);
    // constrain hull to contain origin point
    candidate_points.emplace_back(_node_info.coordinates.size(), 0);
    auto neighbor_points = QuickHull::convexHull(candidate_points);
    for (size_t i = 0; i < candidate_peers.size(); ++i) {
        if (neighbor_points.count(candidate_points[i])) {
            selected_peers.emplace_back(candidate_peers[i]->node_info.address);
            _recipients.emplace_back(candidate_peers[i]->node_info.address);
        }
    }
    // remove duplicates from recipients list
    std::sort(_recipients.begin(), _recipients.end());
    _recipients.erase(std::unique(_recipients.begin(), _recipients.end()), _recipients.end());
    // swap recipients list with output and clear;
    _recipients.swap(recipients);
    // tick node sequence
    ++_node_info.sequence;
    IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(PEER_SELECTIONS_GENERATED)));
}

}  // namespace vsm
