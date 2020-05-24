#include <zmq.hpp>
#include <zmq_timers.hpp>
#include <vsm/msg_types_generated.h>

#include <vsm/transport_interface.hpp>
#include <vsm/node.hpp>

#include <unordered_map>
#include <cassert>
#include <iostream>

namespace fbs = flatbuffers;

namespace vsm {

class ZmqNode {
public:
    struct Config {
        uint16_t tcp_port = 11222;
        uint16_t udp_port = 11223;
        uint16_t beacon_interval = 1000;
    };

    struct Peer {};

    ZmqNode(const Config& config)
            : _config(config)
            , _req(_zmq_ctx, zmq::socket_type::router)
            , _rep(_zmq_ctx, zmq::socket_type::router)
            , _pub(_zmq_ctx, zmq::socket_type::radio)
            , _sub(_zmq_ctx, zmq::socket_type::dish)
            , _timers()
            , _rx_frames()
            , _poll_items{
                      {_rep, 0, ZMQ_POLLIN, 0},
                      {_req, 0, ZMQ_POLLIN, 0},
                      {_sub, 0, ZMQ_POLLIN, 0},
              } 
            , _poll_callbacks{
               {[this] (std::vector<zmq::message_t>& frames) {requestMsgCb(frames);}},
               {[this] (std::vector<zmq::message_t>& frames) {responseMsgCb(frames);}},
               {[this] (std::vector<zmq::message_t>& frames) {subscribeMsgCb(frames);}},
             }
             , _fbs(256) {
        _rep.set(zmq::sockopt::router_notify, ZMQ_NOTIFY_CONNECT | ZMQ_NOTIFY_DISCONNECT);
        _rep.bind("tcp://*:" + std::to_string(_config.tcp_port));
        _sub.bind("udp://*:" + std::to_string(_config.udp_port));
        _timers.add(_config.beacon_interval, [this](int) { beaconTmrCb(); });
    }

    void connect(const std::string& addr) {
        _req.set(zmq::sockopt::router_notify, ZMQ_NOTIFY_CONNECT | ZMQ_NOTIFY_DISCONNECT);
        _req.connect(addr);
        std::cout << "connect to " << addr << std::endl;
    }

    void subscribeMsgCb(std::vector<zmq::message_t>& frames) {
        // std::cout << "got message: " << frames.front().to_string()
        //          << " from: " << frames.front().gets("Peer-Address") << std::endl;
    }

    void requestMsgCb(std::vector<zmq::message_t>& frames) {
        assert(frames.size() > 1);
        puts("got request");
        for (auto& frame : frames) {
            puts(frame.str().c_str());
        }
        if (frames.size() == 2) {
            uint32_t route_id =
                    *reinterpret_cast<uint32_t*>(static_cast<char*>(frames.front().data()) + 1);
            _inbound_peers[route_id];
        } else {
            puts(frames.back().gets("Peer-Address"));
        }
#if 0
        msg::TestStruct test_struct(99);
        auto struct_offset = _fbs.CreateStruct(test_struct);
        _fbs.Finish(struct_offset.Union());
        frames.emplace_back(_fbs.GetBufferPointer(), _fbs.GetSize());
        send_msg(_rep, frames);
#endif
    }

    void responseMsgCb(std::vector<zmq::message_t>& frames) {
        puts("got response");
        for (auto& frame : frames) {
            puts(frame.str().c_str());
        }
        if (frames.size() == 2) {
            uint32_t route_id =
                    *reinterpret_cast<uint32_t*>(static_cast<char*>(frames.front().data()) + 1);
            _outbound_peers[route_id];
        } else {
            // puts(frames.back().gets("Peer-Address"));
            // auto msg = fbs::GetRoot<msg::StructUnion>(frames.back().data());
            // std::cout << "deserialized: " << msg->test_struct() << std::endl;
        }
    }

    void beaconTmrCb() {
        std::cout << "beacon " << std::endl;

        //_builder.Clear();
        // auto request = CreateRequestMsg(_builder, RequestType::CONNECT);
        //_builder.Finish(request);

        for (const auto& peer : _outbound_peers) {
            std::vector<zmq::message_t> frames;
            frames.emplace_back(5);
            uint8_t* route_id = static_cast<uint8_t*>(frames.front().data());
            route_id[0] = 0;
            std::memcpy(route_id + 1, &peer.first, 4);
            frames.emplace_back();
            frames.emplace_back();
            // frames.emplace_back(_builder.GetBufferPointer(), _builder.GetSize());
            puts("send request");
            for (auto& frame : frames) {
                puts(frame.str().c_str());
            }
            send_msg(_req, frames);
        }
    }

    void run() {
        while (true) {
            _timers.execute();
            zmq::poll(_poll_items, std::chrono::milliseconds(_timers.timeout()));
            for (const auto& poll_item : _poll_items) {
                if (poll_item.revents & ZMQ_POLLIN) {
                    _rx_frames.clear();
                    do {
                        _rx_frames.emplace_back();
                        zmq_recvmsg(poll_item.socket, _rx_frames.back().handle(), ZMQ_DONTWAIT);
                    } while (_rx_frames.back().more());
                    _poll_callbacks[&poll_item - &_poll_items.front()](_rx_frames);
                }
            }
        }
    }

private:
    int send_msg(zmq::socket_t& socket, std::vector<zmq::message_t>& frames) {
        int bytes_sent = 0;
        for (auto& frame : frames) {
            auto flags = zmq::send_flags::dontwait;
            if (&frame != &frames.back()) {
                flags = flags | zmq::send_flags::sndmore;
            }
            auto ret = socket.send(frame, flags);
            if (!ret.has_value()) {
                return -1;
            }
            bytes_sent += ret.value();
        }
        return bytes_sent;
    };

    Config _config;
    zmq::context_t _zmq_ctx;
    zmq::socket_t _req;
    zmq::socket_t _rep;
    zmq::socket_t _pub;
    zmq::socket_t _sub;
    zmq::timers_t _timers;
    std::vector<zmq::message_t> _rx_frames;
    std::vector<zmq::pollitem_t> _poll_items;
    std::vector<std::function<void(std::vector<zmq::message_t>&)>> _poll_callbacks;
    std::unordered_map<uint32_t, Peer> _inbound_peers;
    std::unordered_map<uint32_t, Peer> _outbound_peers;
    fbs::FlatBufferBuilder _fbs;
};

}  // namespace vsm

int main(int argc, char* argv[]) {
    vsm::ZmqNode::Config node_config;
    if (argc > 1) {
        node_config.tcp_port = std::stoi(argv[1]);
    }
    if (argc > 2) {
        node_config.udp_port = std::stoi(argv[2]);
    }
    std::cout << "listening on tcp:" << node_config.tcp_port << " udp:" << node_config.udp_port
              << std::endl;
    vsm::ZmqNode node(node_config);
    if (argc > 3) {
        std::cout << "connecting to " << argv[3] << std::endl;
        node.connect(argv[3]);
    }
    node.run();
}
