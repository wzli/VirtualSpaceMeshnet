#pragma once

#include <vsm/logger.hpp>
#include <vsm/ego_sphere.hpp>
#include <vsm/peer_tracker.hpp>
#include <vsm/time_sync.hpp>
#include <vsm/transport.hpp>

#include <mutex>

namespace vsm {

class MeshNode {
public:
    enum ErrorType {
        START_OFFSET = 100,
        // Error
        NO_TRANSPORT_SPECIFIED,
        ADD_MESSAGE_HANDLER_FAIL,
        ADD_TIMER_FAIL,
        MESSAGE_VERIFY_FAIL,
        // Info
        INITIALIZED,
        PEER_UPDATES_SENT,
        ENTITY_UPDATES_SENT,
        // Debug
        ENTITY_UPDATES_FORWARDED,
        // Trace
        SOURCE_UPDATE_RECEIVED,
        PEER_UPDATES_RECEIVED,
        ENTITY_UPDATES_RECEIVED,
        TIME_SYNCED,
    };

    struct Config {
        msecs peer_update_interval = msecs(1000);
        msecs entity_expiry_interval = msecs(1000);
        EgoSphere::Config ego_sphere;
        PeerTracker::Config peer_tracker;
        std::shared_ptr<Transport> transport;
        std::shared_ptr<Logger> logger;
        std::function<msecs(void)> local_clock = []() {
            return std::chrono::duration_cast<msecs>(
                    std::chrono::steady_clock::now().time_since_epoch());
        };
    };

    // no copy or move since there are callbacks anchored
    MeshNode(const MeshNode&) = delete;
    MeshNode& operator=(const MeshNode&) = delete;
    MeshNode(MeshNode&&) = delete;
    MeshNode& operator=(MeshNode&&) = delete;

    MeshNode(Config config);

    // entities interface
    using EntitiesCallback = std::function<void(const EgoSphere::EntityLookup& entities)>;
    void readEntities(const EntitiesCallback& entities_callback) {
        const std::lock_guard<std::mutex> lock(_entities_mutex);
        entities_callback(_ego_sphere.getEntities());
    }
    void updateEntities(const std::vector<EntityT>& entity_updates);

    // accesors
    PeerTracker& getPeerTracker() { return _peer_tracker; }
    const PeerTracker& getPeerTracker() const { return _peer_tracker; }

    TimeSync<msecs>& getTimeSync() { return _time_sync; }
    const TimeSync<msecs>& getTimeSync() const { return _time_sync; }

    Transport& getTransport() { return *_transport; }
    const Transport& getTransport() const { return *_transport; }

    Logger* getLogger() { return _logger.get(); }
    const Logger* getLogger() const { return _logger.get(); }

    const std::vector<std::string>& getConnectedPeers() const { return _connected_peers; }

private:
    // internall callbacks
    void forwardEntityUpdates(fb::FlatBufferBuilder& fbb, const Message* msg);
    void sendPeerUpdates();
    void receiveMessageHandler(const void* buffer, size_t len);

    EgoSphere _ego_sphere;
    PeerTracker _peer_tracker;
    TimeSync<msecs> _time_sync;
    std::shared_ptr<Transport> _transport;
    std::shared_ptr<Logger> _logger;
    flatbuffers::FlatBufferBuilder _fbb;
    std::vector<std::string> _connected_peers;
    std::vector<std::string> _recipients_buffer;
    std::mutex _entities_mutex;
    std::mutex _transmit_mutex;
};

}  // namespace vsm
