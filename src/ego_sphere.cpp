#include <vsm/ego_sphere.hpp>

namespace vsm {

int EgoSphere::receiveEntityUpdates(
        const Message* msg, const PeerTracker& peer_tracker, msecs current_time) {
    if (!msg || !msg->entities()) {
        return 0;
    }
    int entities_updated = 0;
    for (auto entity : *msg->entities()) {
        // reject if entity is missing name
        if (!entity->name()) {
            continue;
        }
        // reject if entity already expired
        if (entity->expiry() < current_time.count()) {
            continue;
        }
        auto entity_record = _entities.find(entity->name()->str());
        // reject if timestamp for entity already exist
        if (entity_record != _entities.end() &&
                entity_record->second.timestamps.count(msg->timestamp())) {
            continue;
        }
        // reject if entity range is exceeded
        if (entity->range() && entity->coordinates() &&
                (distanceSqr(*entity->coordinates(), peer_tracker.getNodeInfo().coordinates) >
                        entity->range() * entity->range())) {
            continue;
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
            entity = _entities.erase(entity);
        } else {
            ++entity;
        }
    }
}

}  // namespace vsm
