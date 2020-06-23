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
        // find previous record of entity
        auto entity_record = _entities.find(entity->name()->c_str());
        if (entity_record != _entities.end()) {
            // reject if timestamp for entity was already received
            if (entity_record->second.timestamps.count(msg->timestamp())) {
                IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_ALREADY_RECEIVED)), entity);
                continue;
            }
            // use filter of original entity if it exists
            filter = entity_record->second.entity.filter;
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
            deleteEntity(entity->name()->c_str(), current_time, msg->source());
            IF_PTR(_logger, log, Logger::DEBUG, Error("Received " STRERR(ENTITY_EXPIRED)), entity);
            continue;
        }
        // reject and delete if entity range is exceeded
        if (entity->range() && (entity->range() * entity->range() <
                                       distanceSqr(*entity->coordinates(),
                                               peer_tracker.getNodeInfo().coordinates))) {
            deleteEntity(entity->name()->c_str(), current_time, msg->source());
            IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_RANGE_EXCEEDED)), entity);
            continue;
        }
        // checks pass, proceed to update entity
        if (entity_record == _entities.end()) {
            EntityT new_entity;
            entity->UnPackTo(&new_entity);
            _entities[entity->name()->c_str()] = {std::move(new_entity), {msg->timestamp()}};
        } else {
            // insert timestamp and clear half of timestamp lookup when full
            auto& stamps = entity_record->second.timestamps;
            stamps.insert(msg->timestamp());
            if (stamps.size() > _config.timestamp_lookup_size) {
                stamps.erase(stamps.begin(), std::next(stamps.begin(), stamps.size() / 2));
            }
        }
        ++entities_updated;
    }
    return entities_updated;
}  // namespace vsm

bool EgoSphere::deleteEntity(const char* name, msecs current_time, const NodeInfo* source) {
    if (!name) {
        return false;
    }
    auto entity_record = _entities.find(name);
    if (entity_record == _entities.end()) {
        return false;
    }
    IF_FUNC(_entity_update_handler, nullptr, &entity_record->second.entity, source, current_time);
    _entities.erase(entity_record);
    return true;
}

void EgoSphere::expireEntities(msecs current_time) {
    for (auto entity = _entities.begin(); entity != _entities.end();) {
        if (entity->second.entity.expiry <= current_time.count()) {
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
