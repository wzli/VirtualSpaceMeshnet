#pragma once

#include <vsm/logger.hpp>
#include <vsm/peer_tracker.hpp>
#include <vsm/time_sync.hpp>
#include <vsm/transport.hpp>

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
        // Trace
        SOURCE_UPDATE_RECEIVED,
        PEER_UPDATES_RECEIVED,
        STATE_UPDATES_RECEIVED,
    };

    struct Config {
        msecs peer_update_interval = msecs(1000);
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

    void sendPeerUpdates();

    PeerTracker& getPeerTracker() { return _peer_tracker; }
    TimeSync<msecs>& getTimeSync() { return _time_sync; }
    Transport& getTransport() { return *_transport; }
    Logger* getLogger() { return _logger.get(); }

    const std::vector<std::string>& getConnectedPeers() const { return _connected_peers; }

private:
    void receiveMessageHandler(const void* buffer, size_t len);

    PeerTracker _peer_tracker;
    TimeSync<msecs> _time_sync;
    std::shared_ptr<Transport> _transport;
    std::shared_ptr<Logger> _logger;
    flatbuffers::FlatBufferBuilder _fbb;
    std::vector<std::string> _connected_peers;
    std::vector<std::string> _recipients_buffer;
};

}  // namespace vsm
