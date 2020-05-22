#include <zmq.hpp>
#include "zmq_timers.hpp"
#include <iostream>

namespace vsmn {

class ZmqNode {
public:
    struct Config {
        uint16_t tcp_port = 11222;
        uint16_t udp_port = 11223;
        uint16_t beacon_interval = 1000;
    };

    ZmqNode(const Config& config)
            : _config(config)
            , _req(_ctx, zmq::socket_type::req)
            , _rep(_ctx, zmq::socket_type::rep)
            , _pub(_ctx, zmq::socket_type::radio)
            , _sub(_ctx, zmq::socket_type::dish)
            , _timers()
            , _rx_msg()
            , _poll_items{
                      {_rep, 0, ZMQ_POLLIN, 0},
                      {_req, 0, ZMQ_POLLIN, 0},
                      {_sub, 0, ZMQ_POLLIN, 0},
              } 
            , _poll_callbacks{
               {[this] (zmq::message_t& msg) {requestMsgCb(msg);}},
               {[this] (zmq::message_t& msg) {responseMsgCb(msg);}},
               {[this] (zmq::message_t& msg) {subscribeMsgCb(msg);}},
             } {
        _rep.bind("tcp://*:" + std::to_string(_config.tcp_port));
        _sub.bind("udp://*:" + std::to_string(_config.udp_port));
        _timers.add(_config.beacon_interval, [this](int) { beaconTmrCb(); });
    }

    void connect(const std::string& addr) {
        _req.connect(addr);
        std::cout << "connect to " << addr << std::endl;
    }

    void subscribeMsgCb(zmq::message_t& msg) {
        std::cout << "got message: " << msg.to_string() << " from: " << msg.gets("Peer-Address")
                  << std::endl;
    }

    void requestMsgCb(zmq::message_t& msg) {
        std::cout << "got request: " << msg.to_string() << " from: " << msg.gets("Peer-Address")
                  << std::endl;
        zmq::message_t rep_msg(5);
        memcpy(rep_msg.data(), "World", 5);
        std::cout << "send response: " << rep_msg.to_string() << std::endl;
        _rep.send(rep_msg, zmq::send_flags::dontwait);
    }

    void responseMsgCb(zmq::message_t& msg) {
        std::cout << "got response: " << msg.to_string() << " from: " << msg.gets("Peer-Address")
                  << std::endl;
    }

    void beaconTmrCb() {
        std::cout << "beacon " << std::endl;
        zmq::message_t req_msg(6);
        memcpy(req_msg.data(), "Hello", 6);
        std::cout << "send request: " << req_msg.to_string() << std::endl;
        _req.send(req_msg, zmq::send_flags::dontwait);
    }

    void run() {
        while (true) {
            _timers.execute();
            zmq::poll(_poll_items, std::chrono::milliseconds(_timers.timeout()));
            for (const auto& poll_item : _poll_items) {
                if (poll_item.revents & ZMQ_POLLIN) {
                    zmq_recvmsg(poll_item.socket, _rx_msg.handle(), ZMQ_DONTWAIT);
                    _poll_callbacks[&poll_item - &_poll_items.front()](_rx_msg);
                }
            }
        }
    }

private:
    Config _config;
    zmq::context_t _ctx;
    zmq::socket_t _req;
    zmq::socket_t _rep;
    zmq::socket_t _pub;
    zmq::socket_t _sub;
    zmq::timers_t _timers;
    zmq::message_t _rx_msg;
    std::vector<zmq::pollitem_t> _poll_items;
    std::vector<std::function<void(zmq::message_t&)>> _poll_callbacks;
};

}  // namespace vsmn

int main(int argc, char* argv[]) {
    vsmn::ZmqNode::Config node_config;
    if (argc > 1) {
        node_config.tcp_port = std::stoi(argv[1]);
    }
    if (argc > 2) {
        node_config.udp_port = std::stoi(argv[2]);
    }
    std::cout << "listening on tcp:" << node_config.tcp_port << " udp:" << node_config.udp_port
              << std::endl;
    vsmn::ZmqNode node(node_config);
    if (argc > 3) {
        std::cout << "connecting to " << argv[3] << std::endl;
        node.connect(argv[3]);
    }
    node.run();
}
