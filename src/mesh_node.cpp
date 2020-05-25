#include <vsm/mesh_node.hpp>

namespace vsm {

namespace fbs = flatbuffers;

MeshNode::MeshNode(std::unique_ptr<Transport> transport)
        : _transport(std::move(transport))
        , _peer_manager() {}

int MeshNode::init(const Config& config) {
    if (int error = _transport->addReceiver(
                [this](const void* buffer, size_t len) { recvStateUpdates(buffer, len); }, "")) {
        return error;
    }
    if (int error = _transport->addReceiver(
                [this](const void* buffer, size_t len) { recvPeerUpdates(buffer, len); }, "B")) {
        return error;
    }
    if (int error = _transport->addTimer(
                config.beacon_interval, [this](int) { _peer_manager.generateBeacon(); })) {
        return error;
    }
    return 0;
}

const Message* MeshNode::getMessage(const void* buffer, size_t len) {
    const uint8_t* buf = static_cast<const uint8_t*>(buffer);
    auto msg = fbs::GetRoot<Message>(buf);
    fbs::Verifier verifier(buf, len);
    return msg->Verify(verifier) ? msg : nullptr;
}

void MeshNode::recvPeerUpdates(const void* buffer, size_t len) {
    auto msg = getMessage(buffer, len);
    if (!msg) {
        return;
    }
    for (auto node_info : *msg->peers()) {
        _peer_manager.updatePeer(node_info);
    }
}

void MeshNode::recvStateUpdates(const void* buffer, size_t len) {
    auto msg = getMessage(buffer, len);
    if (!msg) {
        return;
    }
    for (auto state : *msg->states()) {
    }
}

}  // namespace vsm
