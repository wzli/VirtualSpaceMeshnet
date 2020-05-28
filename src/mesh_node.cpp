#include <vsm/mesh_node.hpp>

namespace vsm {

using namespace flatbuffers;

MeshNode::MeshNode(Config config)
        : _stats()
        , _peer_manager(std::move(config.peer_manager))
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
    auto ranked_peers = _peer_manager.updatePeerRankings(_fbb, _recipients_buffer, _current_time);
    // create iterators for updating connections
    struct BackAssigner {
        using value_type = std::string;
        void push_back(const std::string& address) { assign(address); };
        std::function<void(const std::string&)> assign;
    };
    BackAssigner disconnector{[this](const std::string& addr) { _transport->disconnect(addr); }};
    BackAssigner connector{[this](const std::string& addr) { _transport->connect(addr); }};
    // update transport connections
    std::sort(_recipients_buffer.begin(), _recipients_buffer.end());
    std::set_difference(_connected_peers.begin(), _connected_peers.end(),
            _recipients_buffer.begin(), _recipients_buffer.end(), std::back_inserter(disconnector));
    std::set_difference(_recipients_buffer.begin(), _recipients_buffer.end(),
            _connected_peers.begin(), _connected_peers.end(), std::back_inserter(connector));
    _connected_peers.swap(_recipients_buffer);
    // add source info to peer vector
    auto source = NodeInfo::Pack(_fbb, &_peer_manager.getNodeInfo());
    ranked_peers.emplace_back(source);
    // write message
    MessageBuilder msg_builder(_fbb);
    msg_builder.add_source(source);
    msg_builder.add_peers(_fbb.CreateVector(ranked_peers));
    auto msg = msg_builder.Finish();
    _fbb.Finish(msg);
    // send message
    _transport->transmit(_fbb.GetBufferPointer(), _fbb.GetSize());
    IF_PTR(_logger, log, Logger::TRACE, Error("Peer updates sent.", PEER_UPDATES_SENT));
}

void MeshNode::receiveMessageHandler(const void* buffer, size_t len) {
    const uint8_t* buf = static_cast<const uint8_t*>(buffer);
    auto msg = GetRoot<Message>(buf);
    Verifier verifier(buf, len);
#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
    len = verifier.GetComputedSize();
#endif
    if (!msg->Verify(verifier)) {
        Error error("Failed to verify message.", MESSAGE_VERIFY_FAIL);
        IF_PTR(_logger, log, Logger::WARN, error, buffer, len);
        ++_stats.message_verify_failures;
        return;
    }
    if (msg->peers()) {
        _peer_manager.receivePeerUpdates(msg, _current_time);
        Error error("Peer updates received.", PEER_UPDATES_RECEIVED);
        IF_PTR(_logger, log, Logger::TRACE, error, buffer, len);
        ++_stats.peer_updates_received;
    }
    if (msg->states()) {
        //    for (auto state : *msg->states()) {
        //    }
        Error error("State updates received.", STATE_UPDATES_RECEIVED);
        IF_PTR(_logger, log, Logger::TRACE, error, buffer, len);
        ++_stats.state_updates_received;
    }
}

}  // namespace vsm
