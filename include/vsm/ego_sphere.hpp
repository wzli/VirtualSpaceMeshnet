#pragma once
#include <vsm/peer_tracker.hpp>

#include <functional>
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
        std::set<uint32_t> timestamps;
    };

    using EntityLookup = std::unordered_map<std::string, EntityRecord>;

    EgoSphere(Config config)
            : _entity_update_handler(std::move(config.entity_update_handler)){};

    int receiveEntityUpdates(
            const Message* msg, const PeerTracker& peer_tracker, msecs current_time);

    void expireEntities(msecs current_time);

private:
    EntityLookup _entities;
    EntityUpdateHandler _entity_update_handler;
};

}  // namespace vsm
