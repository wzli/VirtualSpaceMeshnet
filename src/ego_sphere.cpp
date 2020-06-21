#include <vsm/ego_sphere.hpp>

namespace vsm {

void EgoSphere::expireEntities(msecs until) {
    while (_expiry_queue.top().expiry < until) {
        const auto found = _entities.find(_expiry_queue.top().entity_id);
        ;
        _expiry_queue.pop();
        // skip if entity doesn't exist (already deleted)
        if (found == _entities.end()) {
            continue;
        }
        const Entity& entity = found->second.entity;
        // skip if entity expiry is not met (it got updated)
        if (entity.expiry() >= until.count()) {
            continue;
        }
        // notify about the deletion
        if (_entity_update_handler) {
            _entity_update_handler(nullptr, &entity, nullptr, until);
        }
        // delete the entity
        _entities.erase(found);
    }
}

}  // namespace vsm
