#include <vsm/ego_sphere.hpp>
#include <algorithm>

namespace vsm {

std::vector<fb::Offset<Entity>> EgoSphere::receiveEntityUpdates(fb::FlatBufferBuilder& fbb,
        const Message* msg, const PeerTracker& peer_tracker,
        const std::vector<std::string>& connected_peers, msecs current_time) {
    std::vector<fb::Offset<Entity>> forward_entities;
    if (!msg || !msg->entities()) {
        return forward_entities;
    }
    // unpack message source
    std::unique_ptr<NodeInfoT> source;
    if (msg->source()) {
        source = std::make_unique<NodeInfoT>();
        msg->source()->UnPackTo(source.get());
    }
    for (auto entity : *msg->entities()) {
        // reject if entity is missing name
        if (!entity->name()) {
            IF_PTR(_logger, log, Logger::WARN, Error(STRERR(ENTITY_NAME_MISSING)), entity);
            continue;
        }
        std::string name = entity->name()->str();
        // reject if entity timestamp was already received
        if (_timestamps.count({name, msecs(msg->timestamp())})) {
            IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_ALREADY_RECEIVED)), entity);
            continue;
        }
        // find previous record of entity
        auto old_entity = _entities.find(name);
        // don't filter if source is self, otherwise use filter of original entity if it exists
        Filter filter = source && source->address == peer_tracker.getNodeInfo().address
                                ? Filter::ALL
                                : old_entity == _entities.end() ? entity->filter()
                                                                : old_entity->second.filter;
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
                if (!source || source->address.empty()) {
                    IF_PTR(_logger, log, Logger::WARN, Error(STRERR(MESSAGE_SOURCE_INVALID)), msg);
                    continue;
                }
                if (peer_tracker.nearestPeer(*entity->coordinates(), connected_peers)
                                .node_info.address != source->address) {
                    IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_NEAREST_FILTERED)),
                            entity);
                    continue;
                }
                break;
        }
        // insert entity timestamp once filter passes
        insertEntityTimestamp(name, msecs(msg->timestamp()));
        // reject and delete if entity already expired
        if (entity->expiry() <= current_time.count()) {
            deleteEntity(name, current_time, source.get());
            IF_PTR(_logger, log, Logger::DEBUG, Error("Received " STRERR(ENTITY_EXPIRED)), entity);
            continue;
        }
        // reject and delete if entity range is exceeded
        if (entity->range() && (entity->range() * entity->range() <
                                       distanceSqr(*entity->coordinates(),
                                               peer_tracker.getNodeInfo().coordinates))) {
            deleteEntity(name, current_time, source.get());
            IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_RANGE_EXCEEDED)), entity);
            continue;
        }
        // checks pass, proceed to update entity
        EntityT new_entity;
        entity->UnPackTo(&new_entity);
        if (old_entity == _entities.end()) {
            // create new entity
            IF_FUNC(_entity_update_handler, &new_entity, nullptr, source.get(), current_time);
            old_entity = _entities.emplace(name, std::move(new_entity)).first;
            IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_CREATED)), entity);
        } else {
            // update existing entity
            IF_FUNC(_entity_update_handler, &new_entity, &old_entity->second, source.get(),
                    current_time);
            old_entity->second = std::move(new_entity);
            IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_UPDATED)), entity);
        }
        // write updated entity to forward message
        forward_entities.emplace_back(Entity::Pack(fbb, &old_entity->second));
    }
    return forward_entities;
}

bool EgoSphere::deleteEntity(const std::string& name, msecs current_time, const NodeInfoT* source) {
    auto entity = _entities.find(name);
    if (entity == _entities.end()) {
        return false;
    }
    IF_FUNC(_entity_update_handler, nullptr, &entity->second, source, current_time);
    IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_DELETED)), &entity->second);
    _entities.erase(entity);
    return true;
}

void EgoSphere::expireEntities(msecs current_time, const NodeInfoT* source) {
    for (auto entity = _entities.begin(); entity != _entities.end();) {
        if (entity->second.expiry <= current_time.count()) {
            IF_FUNC(_entity_update_handler, nullptr, &entity->second, source, current_time);
            IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_EXPIRED)), &entity->second);
            entity = _entities.erase(entity);
        } else {
            ++entity;
        }
    }
}

bool EgoSphere::insertEntityTimestamp(std::string name, msecs timestamp) {
    if (!_timestamps.insert({std::move(name), timestamp}).second) {
        return false;
    }
    // clear half of the lookup table when full
    if (_timestamps.size() > _config.timestamp_lookup_size) {
        _timestamps.erase(
                _timestamps.begin(), std::next(_timestamps.begin(), _timestamps.size() / 2));
        IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_TIMESTAMPS_TRIMMED)));
    }
    return true;
}

}  // namespace vsm
