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
        // Warn
        ENTITY_MISSING_NAME,
        // Debug
        ENTITY_EXPIRED,
        // Trace
    };

    struct Config {
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

    void expireEntities(msecs current_time);

    Logger* getLogger() { return _logger.get(); }
    const Logger* getLogger() const { return _logger.get(); }

private:
    EntityLookup _entities;
    EntityUpdateHandler _entity_update_handler;
    std::shared_ptr<Logger> _logger;
};

}  // namespace vsm
