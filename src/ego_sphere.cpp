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
        const Entity* old_entity = nullptr;
        // find previous record of entity
        auto entity_record = _entities.find(entity->name()->c_str());
        if (entity_record != _entities.end()) {
            // reject if timestamp for entity was already received
            if (entity_record->second.timestamps.count(msg->timestamp())) {
                IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_ALREADY_RECEIVED)), entity);
                continue;
            }
            // use filter of original entity if it exists
            old_entity = &(entity_record->second.entity);
            filter = old_entity->filter();
        }
        // reject if entity is missing coordinates and range or proximity filter is enabled
        if (!entity->coordinates() && (entity->range() || filter == Filter::NEAREST)) {
            IF_PTR(_logger, log, Logger::WARN, Error(STRERR(ENTITY_COORDINATES_MISSING)), entity);
            continue;
        }
        // reject based on filter selection
        switch (filter) {
            case Filter::ALL:
                break;
            case Filter::NEAREST:
                if (!msg->source() || !msg->source()->address()) {
                    IF_PTR(_logger, log, Logger::WARN, Error(STRERR(MESSAGE_SOURCE_INVALID)), msg);
                    continue;
                }
                if (peer_tracker.nearestPeer(*entity->coordinates(), connected_peers)
                                .node_info.address != msg->source()->address()->c_str()) {
                    IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_NEAREST_FILTERED)),
                            entity);
                    continue;
                }
                break;
        }
        // reject and delete if entity already expired
        if (entity->expiry() <= current_time.count()) {
            deleteEntity(old_entity, current_time, msg->source());
            IF_PTR(_logger, log, Logger::DEBUG, Error("Received " STRERR(ENTITY_EXPIRED)), entity);
            continue;
        }
        // reject and delete if entity range is exceeded
        if (entity->range() && (entity->range() * entity->range() <
                                       distanceSqr(*entity->coordinates(),
                                               peer_tracker.getNodeInfo().coordinates))) {
            deleteEntity(old_entity, current_time, msg->source());
            IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_RANGE_EXCEEDED)), entity);
            continue;
        }
        // checks pass, update entity
    }
    return entities_updated;
}  // namespace vsm

bool EgoSphere::deleteEntity(const Entity* entity, msecs current_time, const NodeInfo* source) {
    if (!entity || !entity->name()) {
        return false;
    }
    IF_FUNC(_entity_update_handler, nullptr, entity, source, current_time);
    return _entities.erase(entity->name()->c_str());
}

void EgoSphere::expireEntities(msecs current_time) {
    for (auto entity = _entities.begin(); entity != _entities.end();) {
        if (entity->second.entity.expiry() <= current_time.count()) {
            IF_FUNC(_entity_update_handler, nullptr, &entity->second.entity, nullptr, current_time);
            IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_EXPIRED)),
                    &entity->second.entity);
            entity = _entities.erase(entity);
        } else {
            ++entity;
        }
    }
}

}  // namespace vsm
