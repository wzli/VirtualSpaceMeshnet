#pragma once
#include <vsm/logger.hpp>
#include <vsm/msg_types_generated.h>

#include <functional>
#include <queue>
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

    struct EntityExpiry {
        std::string entity_id;
        msecs expiry;
        bool operator<(const EntityExpiry& rhs) const { return this->expiry > rhs.expiry; }
    };

    using EntityLookup = std::unordered_map<std::string, EntityRecord>;

    EgoSphere(Config config)
            : _entity_update_handler(std::move(config.entity_update_handler)){};

    void expireEntities(msecs until);

private:
    EntityUpdateHandler _entity_update_handler;
    EntityLookup _entities;
    std::priority_queue<EntityExpiry> _expiry_queue;
};

}  // namespace vsm
