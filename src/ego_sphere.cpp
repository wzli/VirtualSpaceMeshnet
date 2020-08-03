#include <vsm/ego_sphere.hpp>
#include <algorithm>

namespace vsm {

std::vector<fb::Offset<Entity>> EgoSphere::receiveEntityUpdates(fb::FlatBufferBuilder& fbb,
        const Message* msg, const PeerTracker& peer_tracker,
        const std::vector<std::string>& connected_peers, msecs current_time) {
    std::vector<fb::Offset<Entity>> forward_entities;
    // input checks
    if (!msg || !msg->entities()) {
        return forward_entities;
    }
    if (!msg->source() || !msg->source()->address()) {
        IF_PTR(_logger, log, Logger::WARN, Error(STRERR(MESSAGE_SOURCE_INVALID)), msg);
        return forward_entities;
    }
    // unpack message source
    NodeInfoT source;
    msg->source()->UnPackTo(&source);
    bool from_self = source.address == peer_tracker.getNodeInfo().address;
    // iterate through entities
    for (auto entity : *msg->entities()) {
        // reject if entity is missing coordinates and range or proximity filter is enabled
        if (!entity->coordinates() && (entity->range() || entity->filter() == Filter::NEAREST)) {
            IF_PTR(_logger, log, Logger::WARN, Error(STRERR(ENTITY_COORDINATES_MISSING)), entity);
            continue;
        }
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
        // don't filter if from self, otherwise use filter of original entity if it exists
        Filter filter = from_self
                                ? Filter::ALL
                                : old_entity == _entities.end() ? entity->filter()
                                                                : old_entity->second.entity.filter;
        // nearest filter rejection
        if (filter == Filter::NEAREST) {
            const auto& nearest_peer =
                    old_entity == _entities.end()
                            ? peer_tracker.nearestPeer(*entity->coordinates(), connected_peers)
                            : peer_tracker.nearestPeer(
                                      old_entity->second.entity.coordinates, connected_peers);
            if (nearest_peer.node_info.address != source.address &&
                    (old_entity != _entities.end() ||
                            nearest_peer.node_info.address != peer_tracker.getNodeInfo().address)) {
                Error error(STRERR(ENTITY_NEAREST_FILTERED));
                IF_PTR(_logger, log, Logger::TRACE, error, entity);
                continue;
            }
        }
        // insert entity timestamp once filter passes
        insertEntityTimestamp(name, msecs(msg->timestamp()));
        // create lambda for delete and forward operation
        const auto delete_and_forward = [&]() {
            // if entity exists, delete entity and forward message
            if (deleteEntity(name, source)) {
                EntityT entity_obj;
                entity->UnPackTo(&entity_obj);
                forward_entities.emplace_back(Entity::Pack(fbb, &entity_obj));
            }
        };
        // check if entity already expired
        if (entity->expiry() <= current_time.count()) {
            delete_and_forward();
            IF_PTR(_logger, log, Logger::DEBUG, Error("Received " STRERR(ENTITY_EXPIRED)), entity);
            continue;
        }
        // reject and delete if entity range is exceeded
        if (entity->range() && (entity->range() * entity->range() <
                                       distanceSqr(*entity->coordinates(),
                                               peer_tracker.getNodeInfo().coordinates))) {
            delete_and_forward();
            IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_RANGE_EXCEEDED)), entity);
            continue;
        }
        // checks pass, proceed to update entity
        EntityUpdate new_entity{{}, current_time, msecs(msg->timestamp()), msg->hops()};
        entity->UnPackTo(&new_entity.entity);
        // reject update if handler returns false
        if (_entity_update_handler &&
                !_entity_update_handler(&new_entity,
                        old_entity == _entities.end() ? nullptr : &old_entity->second, source)) {
            continue;
        }
        // update entity
        if (old_entity == _entities.end()) {
            old_entity = _entities.emplace(name, std::move(new_entity)).first;
            IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_CREATED)), entity);
        } else {
            old_entity->second = std::move(new_entity);
            IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_UPDATED)), entity);
        }
        // forward entity until hop limit is reached
        if (!entity->hop_limit() || entity->hop_limit() > msg->hops()) {
            forward_entities.emplace_back(Entity::Pack(fbb, &old_entity->second.entity));
        } else {
            IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_HOPS_EXCEEDED)), entity);
        }
    }
    return forward_entities;
}

bool EgoSphere::deleteEntity(const std::string& name, const NodeInfoT& source) {
    auto entity = _entities.find(name);
    if (entity == _entities.end()) {
        return false;
    }
    if (_entity_update_handler) {
        _entity_update_handler(nullptr, &entity->second, source);
    }
    IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_DELETED)), &entity->second);
    _entities.erase(entity);
    return true;
}

void EgoSphere::expireEntities(msecs current_time, const NodeInfoT& source) {
    for (auto entity = _entities.begin(); entity != _entities.end();) {
        if (entity->second.entity.expiry <= current_time.count()) {
            if (_entity_update_handler) {
                _entity_update_handler(nullptr, &entity->second, source);
            }
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
