#include <vsm/ego_sphere.hpp>
#include <algorithm>

namespace vsm {

int EgoSphere::receiveEntityUpdates(const Message* msg, const PeerTracker& peer_tracker,
        const std::vector<std::string>& connected_peers, msecs current_time) {
    if (!msg || !msg->entities()) {
        return 0;
    }
    int entities_updated = 0;
    for (auto entity : *msg->entities()) {
        // reject if entity is missing name
        if (!entity->name()) {
            IF_PTR(_logger, log, Logger::WARN, Error("Entity missing name.", ENTITY_NAME_MISSING),
                    entity);
            continue;
        }
        // reject if entity already expired
        if (entity->expiry() <= current_time.count()) {
            IF_PTR(_logger, log, Logger::DEBUG, Error("Entity expired.", ENTITY_EXPIRED), entity);
            continue;
        }
        // reject if entity range is exceeded
        if (entity->range() && entity->coordinates() &&
                (distanceSqr(*entity->coordinates(), peer_tracker.getNodeInfo().coordinates) >
                        entity->range() * entity->range())) {
            IF_PTR(_logger, log, Logger::DEBUG,
                    Error("Entity range exceeded.", ENTITY_RANGE_EXCEEDED), entity);
            continue;
        }
        auto entity_record = _entities.find(entity->name()->c_str());
        // reject if timestamp for entity already exist
        if (entity_record != _entities.end() &&
                entity_record->second.timestamps.count(msg->timestamp())) {
            IF_PTR(_logger, log, Logger::TRACE,
                    Error("Entity already received.", ENTITY_ALREADY_RECEIVED), entity);
            continue;
        }
        // apply proximity filter
        if (entity->proximity_filter() && msg->source() && !connected_peers.empty()) {
            float min_radial_cost = std::numeric_limits<float>::max();
            const Peer* min_radial_peer = nullptr;
            const auto calculate_min = [&](const Peer& peer) {
                float radial_cost = peer.radialCost(*entity->coordinates());
                if (radial_cost < min_radial_cost) {
                    min_radial_cost = radial_cost;
                    min_radial_peer = &peer;
                }
            };
            const auto& peers = peer_tracker.getPeers();
            for (const auto& peer_address : connected_peers) {
                auto peer = peers.find(peer_address);
                if (peer != peers.end()) {
                    calculate_min(peer->second);
                }
            }
            calculate_min(peers.at(peer_tracker.getNodeInfo().address));
            if (msg->source()->address()->c_str() != min_radial_peer->node_info.address) {
                continue;
            }
        }
    }
    return entities_updated;
}

void EgoSphere::expireEntities(msecs current_time) {
    for (auto entity = _entities.begin(); entity != _entities.end();) {
        if (entity->second.entity.expiry() <= current_time.count()) {
            // notify about the deletion
            if (_entity_update_handler) {
                _entity_update_handler(nullptr, &entity->second.entity, nullptr, current_time);
            }
            IF_PTR(_logger, log, Logger::DEBUG, Error("Entity expired.", ENTITY_EXPIRED),
                    &entity->second.entity);
            entity = _entities.erase(entity);
        } else {
            ++entity;
        }
    }
}

}  // namespace vsm
