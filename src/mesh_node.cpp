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
        if (_logger) {
            _logger->log(Logger::ERROR, error);
        }
        throw error;
    }
    if (int err_code = _transport->addReceiver(
                [this](const void* buffer, size_t len) { recvStateUpdates(buffer, len); }, "")) {
        Error error("Failed to add state updates receiver.", ADD_MESSAGE_HANDLER_FAIL, err_code);
        if (_logger) {
            _logger->log(Logger::ERROR, error);
        }
        throw error;
    }
    if (int err_code = _transport->addReceiver(
                [this](const void* buffer, size_t len) { recvPeerUpdates(buffer, len); }, "B")) {
        Error error("Failed to add peer updates receiver.", ADD_MESSAGE_HANDLER_FAIL, err_code);
        if (_logger) {
            _logger->log(Logger::ERROR, error);
        }
        throw error;
    }
    if (0 > _transport->addTimer(
                    config.beacon_interval, [this](int) { _peer_manager.generateBeacon(); })) {
        Error error("Failed to add beacon timer.", ADD_TIMER_FAIL);
        if (_logger) {
            _logger->log(Logger::ERROR, error);
        }
        throw error;
    }
    Error error = Error("Mesh node initialized.", INITIALIZED);
    if (_logger) {
        _logger->log(Logger::INFO, error);
    }
}

const Message* MeshNode::getMessage(const void* buffer, size_t& len) {
    const uint8_t* buf = static_cast<const uint8_t*>(buffer);
    auto msg = GetRoot<Message>(buf);
    Verifier verifier(buf, len);
#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
    len = verifier.GetComputedSize();
#endif
    if (msg->Verify(verifier)) {
        return msg;
    } else {
        Error error("Failed to verify message.", MESSAGE_VERIFY_FAIL);
        if (_logger) {
            _logger->log(Logger::WARN, error, buffer, len);
        }
        ++_stats.message_verify_failures;
        return nullptr;
    }
    return msg;
}

void MeshNode::recvPeerUpdates(const void* buffer, size_t len) {
    auto msg = getMessage(buffer, len);
    if (!msg) {
        return;
    }
    Error error("Peer updates received.", PEER_UPDATES_RECEIVED);
    if (_logger) {
        _logger->log(Logger::TRACE, error, buffer, len);
    }
    ++_stats.peer_updates_received;
    for (auto node_info : *msg->peers()) {
        _peer_manager.updatePeer(node_info, len);
    }
}

void MeshNode::recvStateUpdates(const void* buffer, size_t len) {
    auto msg = getMessage(buffer, len);
    if (!msg) {
        return;
    }
    Error error("State updates received.", STATE_UPDATES_RECEIVED);
    if (_logger) {
        _logger->log(Logger::TRACE, error, buffer, len);
    }
    ++_stats.state_updates_received;
    //    for (auto state : *msg->states()) {
    //    }
}

}  // namespace vsm
