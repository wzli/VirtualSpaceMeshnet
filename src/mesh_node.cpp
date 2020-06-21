#include <vsm/mesh_node.hpp>
#include <vsm/time_sync.hpp>

namespace vsm {

using namespace flatbuffers;

MeshNode::MeshNode(Config config)
        : _peer_tracker(std::move(config.peer_tracker), config.logger)
        , _time_sync(std::move(config.local_clock))
        , _transport(std::move(config.transport))
        , _logger(std::move(config.logger)) {
    if (!_transport) {
        Error error("No transport specified.", NO_TRANSPORT_SPECIFIED);
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    if (int err_code = _transport->addReceiver(
                [this](const void* buffer, size_t len) { receiveMessageHandler(buffer, len); })) {
        Error error("Failed to receive message handler.", ADD_MESSAGE_HANDLER_FAIL, err_code);
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    if (0 > _transport->addTimer(config.peer_update_interval, [this](int) { sendPeerUpdates(); })) {
        Error error("Failed to add peer update timer.", ADD_TIMER_FAIL);
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    Error error = Error("Mesh node initialized.", INITIALIZED);
    IF_PTR(_logger, log, Logger::INFO, error);
}

void MeshNode::sendPeerUpdates() {
    _fbb.Clear();
    // get peer rankings
    auto ranked_peers = _peer_tracker.updatePeerRankings(_fbb, _recipients_buffer);
    // create iterators for updating connections
    struct BackAssigner {
        using value_type = std::string;
        void push_back(const std::string& address) { assign(address); };
        std::function<void(const std::string&)> assign;
    };
    BackAssigner disconnector{[this](const std::string& addr) { _transport->disconnect(addr); }};
    BackAssigner connector{[this](const std::string& addr) { _transport->connect(addr); }};
    // update transport connections
    std::set_difference(_connected_peers.begin(), _connected_peers.end(),
            _recipients_buffer.begin(), _recipients_buffer.end(), std::back_inserter(disconnector));
    std::set_difference(_recipients_buffer.begin(), _recipients_buffer.end(),
            _connected_peers.begin(), _connected_peers.end(), std::back_inserter(connector));
    _connected_peers.swap(_recipients_buffer);
    // write message
    MessageBuilder msg_builder(_fbb);
    msg_builder.add_timestamp(_time_sync.getTime().count());
    msg_builder.add_source(NodeInfo::Pack(_fbb, &_peer_tracker.getNodeInfo()));
    msg_builder.add_peers(_fbb.CreateVector(ranked_peers));
    _fbb.Finish(msg_builder.Finish());
    // send message
    _transport->transmit(_fbb.GetBufferPointer(), _fbb.GetSize());
    IF_PTR(_logger, log, Logger::INFO, Error("Peer updates sent.", PEER_UPDATES_SENT));
}

void MeshNode::receiveMessageHandler(const void* buffer, size_t len) {
    auto buf = static_cast<const uint8_t*>(buffer);
    auto msg = GetRoot<Message>(buf);
    Verifier verifier(buf, len);
#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
    len = verifier.GetComputedSize();
#endif
    if (!msg->Verify(verifier)) {
        Error error("Failed to verify message.", MESSAGE_VERIFY_FAIL);
        IF_PTR(_logger, log, Logger::WARN, error, buffer, len);
        return;
    }
    switch (_peer_tracker.updatePeer(msg->source(), true)) {
        case PeerTracker::SUCCESS:
            if (msg->timestamp()) {
                float weight = 1.0f / (1 + _connected_peers.size());
                _time_sync.syncTime(msecs(msg->timestamp()), weight);
            }
            IF_PTR(_logger, log, Logger::TRACE,
                    Error("Source updates received.", SOURCE_UPDATE_RECEIVED), buffer, len);
            // fall through
        case PeerTracker::PEER_IS_NULL:
            if (_peer_tracker.receivePeerUpdates(msg) > 0) {
                Error error("Peer updates received.", PEER_UPDATES_RECEIVED);
                IF_PTR(_logger, log, Logger::TRACE, error, buffer, len);
            }
            break;
        default:
            break;
    }
    if (msg->states()) {
        //    for (auto state : *msg->states()) {
        //    }
        Error error("State updates received.", STATE_UPDATES_RECEIVED);
        IF_PTR(_logger, log, Logger::TRACE, error, buffer, len);
    }
}

}  // namespace vsm
