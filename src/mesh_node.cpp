#include <vsm/mesh_node.hpp>
#include <vsm/time_sync.hpp>

namespace vsm {

using namespace flatbuffers;

MeshNode::MeshNode(Config config)
        : _ego_sphere(std::move(config.ego_sphere), config.logger)
        , _peer_tracker(std::move(config.peer_tracker), config.logger)
        , _time_sync(std::move(config.local_clock))
        , _transport(std::move(config.transport))
        , _logger(std::move(config.logger))
        , _entity_updates_size(config.entity_updates_size) {
    if (!_transport) {
        Error error(STRERR(NO_TRANSPORT_SPECIFIED));
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    // register receive handler
    if (int err_code = _transport->addReceiver(
                [this](const void* buffer, size_t len) { receiveMessageHandler(buffer, len); })) {
        Error error(STRERR(ADD_MESSAGE_HANDLER_FAIL), err_code);
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    // register peer update timer
    if (0 > _transport->addTimer(config.peer_update_interval, [this](int) { sendPeerUpdates(); })) {
        Error error(STRERR(ADD_TIMER_FAIL));
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    // register entity expiry timer
    if (0 > _transport->addTimer(config.entity_expiry_interval, [this](int) {
            const std::lock_guard<std::mutex> lock(_entities_mutex);
            _ego_sphere.expireEntities(_time_sync.getTime(), _peer_tracker.getNodeInfo());
        })) {
        Error error(STRERR(ADD_TIMER_FAIL));
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    IF_PTR(_logger, log, Logger::INFO, Error(STRERR(MeshNode::INITIALIZED)));
}

std::vector<MessageBuffer> MeshNode::updateEntities(
        const std::vector<EntityT>& entities, bool relative_expiry) {
    if (entities.empty()) {
        return {};
    }
    // write message in
    fb::FlatBufferBuilder fbb_in, fbb_out;
    std::vector<MessageBuffer> forwarded_messages;
    std::vector<fb::Offset<Entity>> entity_offsets;
    const auto forward_message = [&]() {
        fbb_in.Finish(CreateMessage(fbb_in,
                _time_sync.getTime().count(),                          // timestamp
                0,                                                     // hops
                NodeInfo::Pack(fbb_in, &_peer_tracker.getNodeInfo()),  // source
                {},                                                    // peers
                fbb_in.CreateVector(entity_offsets)                    // entities
                ));
        auto msg = GetRoot<Message>(fbb_in.GetBufferPointer());
        // increment expiry by timestamp if it's relative
        if (relative_expiry) {
            for (auto entity : *msg->entities()) {
                const_cast<Entity*>(entity)->mutate_expiry(entity->expiry() + msg->timestamp());
            }
        }
        if (forwardEntityUpdates(fbb_out, msg)) {
            forwarded_messages.emplace_back(std::move(fbb_out.Release()));
        }
        fbb_in.Reset();
        fbb_out.Reset();
        entity_offsets.clear();
    };
    for (const auto& entity : entities) {
        entity_offsets.emplace_back(Entity::Pack(fbb_in, &entity));
        if (fbb_in.GetSize() >= _entity_updates_size) {
            forward_message();
        }
    }
    forward_message();
    IF_PTR(_logger, log, Logger::INFO, Error(STRERR(ENTITY_UPDATES_SENT)));
    return forwarded_messages;
}

const Message* MeshNode::forwardEntityUpdates(fb::FlatBufferBuilder& fbb, const Message* msg) {
    fbb.Clear();
    std::vector<fb::Offset<Entity>> forward_entities;
    {
        // lock and update ego sphere entities
        const std::lock_guard<std::mutex> lock(_entities_mutex);
        forward_entities = _ego_sphere.receiveEntityUpdates(
                fbb, msg, _peer_tracker, _connected_peers, _time_sync.getTime());
    }
    if (forward_entities.empty()) {
        return nullptr;
    }
    // write forward message
    fbb.Finish(CreateMessage(fbb,
            msg->timestamp(),                                   // timestamp
            msg->hops() + 1,                                    // hops
            NodeInfo::Pack(fbb, &_peer_tracker.getNodeInfo()),  // source
            {},                                                 // peers
            fbb.CreateVector(forward_entities)                  // entities
            ));
    // don't send message back to the original source
    std::string source_address;
    if (msg->source() && msg->source()->address()) {
        source_address = msg->source()->address()->str();
    }
    int disconnect_error = _transport->disconnect(source_address);
    _transport->transmit(fbb.GetBufferPointer(), fbb.GetSize());
    if (!disconnect_error) {
        _transport->connect(source_address);
    }
    IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_UPDATES_FORWARDED)),
            fbb.GetBufferPointer(), fbb.GetSize());
    return GetRoot<Message>(fbb.GetBufferPointer());
}

void MeshNode::sendPeerUpdates() {
    _fbb.Clear();
    // get peer rankings
    auto ranked_peers = _peer_tracker.updatePeerRankings(_fbb, _recipients_buffer);
    // write message
    _fbb.Finish(CreateMessage(_fbb,
            _time_sync.getTime().count(),                        // timestamp
            1,                                                   // hops
            NodeInfo::Pack(_fbb, &_peer_tracker.getNodeInfo()),  // source
            _fbb.CreateVector(ranked_peers)                      // peers
            ));
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
    // send message
    _transport->transmit(_fbb.GetBufferPointer(), _fbb.GetSize());
    _connected_peers.swap(_recipients_buffer);
    IF_PTR(_logger, log, Logger::INFO, Error(STRERR(PEER_UPDATES_SENT)));
}

void MeshNode::receiveMessageHandler(const void* buffer, size_t len) {
    auto buf = static_cast<const uint8_t*>(buffer);
    auto msg = GetRoot<Message>(buf);
    Verifier verifier(buf, len);
#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
    len = verifier.GetComputedSize();
#endif
    if (!msg->Verify(verifier)) {
        Error error(STRERR(MESSAGE_VERIFY_FAIL));
        IF_PTR(_logger, log, Logger::WARN, error, buffer, len);
        return;
    }
    switch (_peer_tracker.updatePeer(msg->source(), true)) {
        case PeerTracker::SUCCESS:
            if (msg->hops() == 1 && msg->timestamp() > 0) {
                float weight = 1.0f / (1 + _connected_peers.size());
                _time_sync.syncTime(msecs(msg->timestamp()), weight);
                IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(TIME_SYNCED)), buffer, len);
            }
            IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(SOURCE_UPDATE_RECEIVED)), buffer, len);
            // fall through
        case PeerTracker::PEER_IS_NULL:
        case PeerTracker::PEER_ADDRESS_MISSING:
        case PeerTracker::PEER_COORDINATES_MISSING:
            if (_peer_tracker.receivePeerUpdates(msg) > 0) {
                Error error(STRERR(PEER_UPDATES_RECEIVED));
                IF_PTR(_logger, log, Logger::TRACE, error, buffer, len);
            }
            // fall through
        case PeerTracker::SOURCE_SEQUENCE_STALE:
            if (msg->entities()) {
                Error error(STRERR(ENTITY_UPDATES_RECEIVED));
                IF_PTR(_logger, log, Logger::TRACE, error, buffer, len);
                forwardEntityUpdates(_fbb, msg);
            }
            // fall through
        default:
            break;
    }
}

}  // namespace vsm
