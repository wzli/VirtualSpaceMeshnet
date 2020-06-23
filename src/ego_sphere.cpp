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
            IF_PTR(_logger, log, Logger::WARN, Error(STRERR(ENTITY_NAME_MISSING)), entity);
            continue;
        }
        Filter filter = entity->filter();
        auto entity_record = _entities.find(entity->name()->c_str());
        if (entity_record != _entities.end()) {
            // reject if timestamp for entity was already received
            if (entity_record->second.timestamps.count(msg->timestamp())) {
                IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_ALREADY_RECEIVED)), entity);
                continue;
            }
            // use filter of original entity if it exists
            filter = entity_record->second.entity.filter();
        }

        // reject if entity already expired
        if (entity->expiry() <= current_time.count()) {
            IF_PTR(_logger, log, Logger::DEBUG, Error("Received " STRERR(ENTITY_EXPIRED)), entity);
            continue;
        }
        // reject if entity range is exceeded
        if (entity->range() && entity->coordinates() &&
                (distanceSqr(*entity->coordinates(), peer_tracker.getNodeInfo().coordinates) >
                        entity->range() * entity->range())) {
            IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_RANGE_EXCEEDED)), entity);
            continue;
        }
        bool filter_result = true;
        if (filter == Filter::NEAREST) {
            const Peer& nearest_peer =
                    peer_tracker.nearestPeer(*entity->coordinates(), connected_peers);
            if (msg->source()->address()->c_str() == nearest_peer.node_info.address) {
            }
        }
        // apply proximity filter
        //    if (entity->filter() == Filter::NEAREST && msg->source() && !connected_peers.empty())
        //    {
        //    }
    }
    return entities_updated;
}  // namespace vsm

void EgoSphere::expireEntities(msecs current_time) {
    for (auto entity = _entities.begin(); entity != _entities.end();) {
        if (entity->second.entity.expiry() <= current_time.count()) {
            // notify about the deletion
            if (_entity_update_handler) {
                _entity_update_handler(nullptr, &entity->second.entity, nullptr, current_time);
            }
            IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_EXPIRED)),
                    &entity->second.entity);
            entity = _entities.erase(entity);
        } else {
            ++entity;
        }
    }
}

}  // namespace vsm
