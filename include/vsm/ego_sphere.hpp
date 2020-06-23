#pragma once
#include <vsm/logger.hpp>
#include <vsm/msg_types_generated.h>
#include <vsm/peer_tracker.hpp>

#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace vsm {

class EgoSphere {
public:
    using EntityUpdateHandler = std::function<void(Entity* new_version, const Entity* old_version,
            const NodeInfo* source, msecs timestamp)>;

    enum ErrorType {
        SUCCESS = 0,
        START_OFFSET = 300,
        // Error
        // Warn
        MESSAGE_SOURCE_INVALID,
        ENTITY_COORDINATES_MISSING,
        ENTITY_NAME_MISSING,
        // Info
        // Debug
        ENTITY_EXPIRED,
        ENTITY_RANGE_EXCEEDED,
        // Trace
        ENTITY_ALREADY_RECEIVED,
        ENTITY_NEAREST_FILTERED,
    };

    struct Config {
        size_t timestamp_lookup_size = 128;
        EntityUpdateHandler entity_update_handler = nullptr;
    };

    struct EntityRecord {
        Entity entity;
        std::set<uint32_t> timestamps;
    };

    using EntityLookup = std::unordered_map<std::string, EntityRecord>;

    EgoSphere(Config config, std::shared_ptr<Logger> logger = nullptr)
            : _entity_update_handler(std::move(config.entity_update_handler))
            , _logger(std::move(logger)){};

    int receiveEntityUpdates(const Message* msg, const PeerTracker& peer_tracker,
            const std::vector<std::string>& connected_peers, msecs current_time);

    bool deleteEntity(const Entity* entity, msecs current_time, const NodeInfo* source = nullptr);
    void expireEntities(msecs current_time);

    Logger* getLogger() { return _logger.get(); }
    const Logger* getLogger() const { return _logger.get(); }

private:
    EntityLookup _entities;
    EntityUpdateHandler _entity_update_handler;
    std::shared_ptr<Logger> _logger;
};

}  // namespace vsm
