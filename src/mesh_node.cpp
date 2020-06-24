#include <vsm/mesh_node.hpp>
#include <vsm/time_sync.hpp>

namespace vsm {

using namespace flatbuffers;

MeshNode::MeshNode(Config config)
        : _ego_sphere(EgoSphere::Config())
        , _peer_tracker(std::move(config.peer_tracker), config.logger)
        , _time_sync(std::move(config.local_clock))
        , _transport(std::move(config.transport))
        , _logger(std::move(config.logger)) {
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
            _ego_sphere.expireEntities(_time_sync.getTime(), &_peer_tracker.getNodeInfo());
        })) {
        Error error(STRERR(ADD_TIMER_FAIL));
        IF_PTR(_logger, log, Logger::ERROR, error);
        throw error;
    }
    Error error = Error("Mesh node " STRERR(INITIALIZED));
    IF_PTR(_logger, log, Logger::INFO, error);
}

void MeshNode::updateEntities(
        const std::vector<EntityT>& entity_updates, const EntitiesCallback& callback) {
    // record time
    auto timestamp = _time_sync.getTime();
    // write message in
    fb::FlatBufferBuilder fbb_in;
    std::vector<fb::Offset<Entity>> entities;
    for (const auto& entity : entity_updates) {
        entities.emplace_back(Entity::Pack(fbb_in, &entity));
    }
    fbb_in.Finish(CreateMessage(fbb_in,
            timestamp.count(),                                     // timestamp
            0,                                                     // hops
            NodeInfo::Pack(fbb_in, &_peer_tracker.getNodeInfo()),  // source
            {},                                                    // peers
            fbb_in.CreateVector(entities)                          // entities
            ));
    auto msg_in = GetRoot<Message>(fbb_in.GetBufferPointer());
    // write message out
    fb::FlatBufferBuilder fbb_out;
    {
        // lock entities
        const std::lock_guard<std::mutex> lock(_entities_mutex);
        // update entities
        entities = _ego_sphere.receiveEntityUpdates(fbb_out, msg_in, _peer_tracker, {}, timestamp);
        // trigger callback after update
        IF_FUNC(callback, _ego_sphere.getEntities(), timestamp);
    }
    // write message out
    fbb_out.Finish(CreateMessage(fbb_out,
            timestamp.count(),                                      // timestamp
            0,                                                      // hops
            NodeInfo::Pack(fbb_out, &_peer_tracker.getNodeInfo()),  // source
            {},                                                     // peers
            fbb_out.CreateVector(entities)                          // entities
            ));
    {
        // lock transport and send message
        const std::lock_guard<std::mutex> lock(_transmit_mutex);
        _transport->transmit(fbb_out.GetBufferPointer(), fbb_out.GetSize());
    }
}

void MeshNode::sendPeerUpdates() {
    _fbb.Clear();
    // get peer rankings
    auto ranked_peers = _peer_tracker.updatePeerRankings(_fbb, _recipients_buffer);
    // write message
    MessageBuilder msg_builder(_fbb);
    msg_builder.add_timestamp(_time_sync.getTime().count());
    msg_builder.add_source(NodeInfo::Pack(_fbb, &_peer_tracker.getNodeInfo()));
    msg_builder.add_peers(_fbb.CreateVector(ranked_peers));
    _fbb.Finish(msg_builder.Finish());
    // create iterators for updating connections
    struct BackAssigner {
        using value_type = std::string;
        void push_back(const std::string& address) { assign(address); };
        std::function<void(const std::string&)> assign;
    };
    BackAssigner disconnector{[this](const std::string& addr) { _transport->disconnect(addr); }};
    BackAssigner connector{[this](const std::string& addr) { _transport->connect(addr); }};
    {
        // lock transport
        const std::lock_guard<std::mutex> lock(_transmit_mutex);
        // update transport connections
        std::set_difference(_connected_peers.begin(), _connected_peers.end(),
                _recipients_buffer.begin(), _recipients_buffer.end(),
                std::back_inserter(disconnector));
        std::set_difference(_recipients_buffer.begin(), _recipients_buffer.end(),
                _connected_peers.begin(), _connected_peers.end(), std::back_inserter(connector));
        // send message
        _transport->transmit(_fbb.GetBufferPointer(), _fbb.GetSize());
    }
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
            if (msg->hops() == 0 && msg->timestamp() > 0) {
                float weight = 1.0f / (1 + _connected_peers.size());
                _time_sync.syncTime(msecs(msg->timestamp()), weight);
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
        case PeerTracker::SOURCE_SEQUENCE_STALE: {
            IF_PTR(_logger, log, Logger::TRACE, Error(STRERR(ENTITY_UPDATES_RECEIVED)), buffer,
                    len);
            std::vector<fb::Offset<Entity>> forward_entities;
            {
                // lock ego sphere during entity modification
                const std::lock_guard<std::mutex> lock(_entities_mutex);
                forward_entities = _ego_sphere.receiveEntityUpdates(
                        _fbb, msg, _peer_tracker, _connected_peers, _time_sync.getTime());
            }
            if (!forward_entities.empty()) {
                // write forward message
                MessageBuilder msg_builder(_fbb);
                msg_builder.add_timestamp(msg->timestamp());
                msg_builder.add_hops(msg->hops() + 1);
                msg_builder.add_source(NodeInfo::Pack(_fbb, &_peer_tracker.getNodeInfo()));
                msg_builder.add_entities(_fbb.CreateVector(forward_entities));
                _fbb.Finish(msg_builder.Finish());
                {
                    // lock transport and forwrad message
                    const std::lock_guard<std::mutex> lock(_transmit_mutex);
                    _transport->transmit(_fbb.GetBufferPointer(), _fbb.GetSize());
                }
                IF_PTR(_logger, log, Logger::DEBUG, Error(STRERR(ENTITY_UPDATES_SENT)));
            }
        }
            // fall through
        default:
            break;
    }
}

}  // namespace vsm
