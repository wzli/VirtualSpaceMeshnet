#pragma once

#include <vsm/logger.hpp>
#include <vsm/ego_sphere.hpp>
#include <vsm/peer_tracker.hpp>
#include <vsm/time_sync.hpp>
#include <vsm/transport.hpp>

#include <mutex>

namespace vsm {

using EntitiesCallback = std::function<void(const EgoSphere::EntityLookup& entities)>;

struct MessageBuffer : public fb::DetachedBuffer {
    using fb::DetachedBuffer::DetachedBuffer;
    MessageBuffer(fb::DetachedBuffer&& buffer)
            : fb::DetachedBuffer(std::move(buffer)){};
    const Message* get() const { return data() ? fb::GetRoot<Message>(data()) : nullptr; }
    Message* get() { return data() ? fb::GetMutableRoot<Message>(data()) : nullptr; }
};

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
        // Debug
        PEER_UPDATES_SENT,
        ENTITY_UPDATES_SENT,
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
        size_t entity_updates_size = 7000;  // message size aprox 600B more may vary
        bool spectator = false;
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
    void readEntities(const EntitiesCallback& entities_callback) {
        const std::lock_guard<std::mutex> lock(_entities_mutex);
        entities_callback(_ego_sphere.getEntities());
    }
    std::vector<MessageBuffer> updateEntities(
            const std::vector<EntityT>& entities, bool relative_expiry = true);
    const Message* forwardEntityUpdates(fb::FlatBufferBuilder& fbb, const Message* msg);

    // accessors (FYI they are not thread safe)
    EgoSphere& getEgoSphere() { return _ego_sphere; }
    const EgoSphere& getEgoSphere() const { return _ego_sphere; }

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
    void sendPeerUpdates();
    void receiveMessageHandler(const void* buffer, size_t len);

    EgoSphere _ego_sphere;
    PeerTracker _peer_tracker;
    TimeSync<msecs> _time_sync;
    std::shared_ptr<Transport> _transport;
    std::shared_ptr<Logger> _logger;
    fb::FlatBufferBuilder _fbb;
    std::vector<fb::Offset<NodeInfo>> _peer_offsets;
    std::vector<std::string> _selected_peers;
    std::vector<std::string> _connected_peers;
    std::vector<std::string> _recipients_buffer;
    std::mutex _entities_mutex;
    size_t _entity_updates_size;
    bool _spectator;
};

}  // namespace vsm
