#include <vsm/ego_sphere.hpp>

namespace vsm {

int EgoSphere::receiveEntityUpdates(const Message* msg) {
    return 0;
}

void EgoSphere::expireEntities(msecs expire_until) {
    for (auto entity = _entities.begin(); entity != _entities.end();) {
        if (entity->second.entity.expiry() <= expire_until.count()) {
            // notify about the deletion
            if (_entity_update_handler) {
                _entity_update_handler(nullptr, &entity->second.entity, nullptr, expire_until);
            }
            entity = _entities.erase(entity);
        } else {
            ++entity;
        }
    }
}

}  // namespace vsm
