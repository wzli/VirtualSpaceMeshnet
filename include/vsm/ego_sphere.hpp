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

namespace fb = flatbuffers;

class EgoSphere {
public:
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
        ENTITY_CREATED,
        ENTITY_DELETED,
        ENTITY_EXPIRED,
        ENTITY_TIMESTAMPS_TRIMMED,
        // Trace
        ENTITY_UPDATED,
        ENTITY_ALREADY_RECEIVED,
        ENTITY_NEAREST_FILTERED,
        ENTITY_RANGE_EXCEEDED,
        ENTITY_HOPS_EXCEEDED,
    };

    struct EntityUpdate {
        EntityT entity;
        msecs receive_timestamp;
        msecs source_timestamp;
        uint32_t hops;
    };

    // return value determines whether to allow update
    using EntityUpdateHandler = std::function<bool(
            EntityUpdate* new_entity, const EntityUpdate* old_entity, const NodeInfoT& source)>;

    struct Config {
        EntityUpdateHandler entity_update_handler = nullptr;
        size_t timestamp_lookup_size = 1024;
    };

    struct EntityTimestamp {
        std::string name;
        msecs timestamp;
        bool operator<(const EntityTimestamp& rhs) const {
            return timestamp == rhs.timestamp ? name < rhs.name : timestamp < rhs.timestamp;
        }
    };

    using EntityLookup = std::unordered_map<std::string, EntityUpdate>;

    EgoSphere(Config config, std::shared_ptr<Logger> logger = nullptr)
            : _config(config)
            , _entity_update_handler(std::move(_config.entity_update_handler))
            , _logger(std::move(logger)){};

    std::vector<fb::Offset<Entity>> receiveEntityUpdates(fb::FlatBufferBuilder& fbb,
            const Message* msg, const PeerTracker& peer_tracker,
            const std::vector<std::string>& connected_peers, msecs current_time);

    bool insertEntityTimestamp(std::string name, msecs timestamp);

    bool deleteEntity(const std::string& name, const NodeInfoT& source);

    void expireEntities(msecs current_time, const NodeInfoT& source);

    // accesors
    void setEntityUpdateHandler(EntityUpdateHandler handler) {
        _entity_update_handler = std::move(handler);
    }
    EntityLookup& getEntities() { return _entities; }
    const EntityLookup& getEntities() const { return _entities; }

    Logger* getLogger() { return _logger.get(); }
    const Logger* getLogger() const { return _logger.get(); }

private:
    Config _config;
    EntityLookup _entities;
    std::set<EntityTimestamp> _timestamps;
    EntityUpdateHandler _entity_update_handler;
    std::shared_ptr<Logger> _logger;
};

}  // namespace vsm
