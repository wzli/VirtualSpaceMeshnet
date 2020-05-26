#include <vsm/mesh_node.hpp>

namespace vsm {

namespace fbs = flatbuffers;

MeshNode::MeshNode(Config config, std::unique_ptr<Transport> transport)
        : _stats()
        , _transport(std::move(transport)) {
    _logger.addLogHandler(config.log_level, std::move(config.log_handler));
    if (int err_code = _transport->addReceiver(
                [this](const void* buffer, size_t len) { recvStateUpdates(buffer, len); }, "")) {
        auto error =
                Error("Failed to add state updates receiver.", ADD_MESSAGE_HANDLER_FAIL, err_code);
        _logger.log(Logger::ERROR, error);
        throw error;
    }
    if (int err_code = _transport->addReceiver(
                [this](const void* buffer, size_t len) { recvPeerUpdates(buffer, len); }, "B")) {
        auto error =
                Error("Failed to add peer updates receiver.", ADD_MESSAGE_HANDLER_FAIL, err_code);
        _logger.log(Logger::ERROR, error);
        throw error;
    }
    if (int err_code = _transport->addTimer(
                config.beacon_interval, [this](int) { _peer_manager.generateBeacon(); })) {
        auto error = Error("Failed to add beacon timer.", ADD_TIMER_FAIL, err_code);
        _logger.log(Logger::ERROR, error);
        throw error;
    }
}

const Message* MeshNode::getMessage(const void* buffer, size_t len) {
    const uint8_t* buf = static_cast<const uint8_t*>(buffer);
    auto msg = fbs::GetRoot<Message>(buf);
    fbs::Verifier verifier(buf, len);
    if (msg->Verify(verifier)) {
        return msg;
    } else {
        auto error = Error("Failed to verify message.", MESSAGE_VERIFY_FAIL);
        _logger.log(Logger::WARN, error, buffer, len);
        ++_stats.message_verify_failures;
        return nullptr;
    }
}

void MeshNode::recvPeerUpdates(const void* buffer, size_t len) {
    auto msg = getMessage(buffer, len);
    if (!msg) {
        return;
    }
    ++_stats.peer_updates_received;
    for (auto node_info : *msg->peers()) {
        _peer_manager.updatePeer(node_info);
    }
}

void MeshNode::recvStateUpdates(const void* buffer, size_t len) {
    auto msg = getMessage(buffer, len);
    if (!msg) {
        return;
    }
    ++_stats.state_updates_received;
    for (auto state : *msg->states()) {
    }
}

}  // namespace vsm
