#pragma once
#include <vsm/logger.hpp>
#include <vsm/msg_types_generated.h>

#include <functional>
#include <set>
#include <unordered_map>

namespace vsm {

class EgoSphere {
public:
    using EntityUpdateHandler = std::function<void(Entity* new_version, const Entity* old_version,
            const NodeInfo* source, msecs timestamp)>;

    struct Config {
        EntityUpdateHandler entity_update_handler = nullptr;
    };

    struct EntityRecord {
        Entity entity;
        std::set<msecs> timestamps;
    };

    using EntityLookup = std::unordered_map<std::string, EntityRecord>;

    EgoSphere(Config config)
            : _entity_update_handler(std::move(config.entity_update_handler)){};

    int receiveEntityUpdates(const Message* msg);

    void expireEntities(msecs expire_until);

private:
    EntityUpdateHandler _entity_update_handler;
    EntityLookup _entities;
};

}  // namespace vsm
