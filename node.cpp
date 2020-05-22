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
            , _pub(_ctx, zmq::socket_type::radio)
            , _sub(_ctx, zmq::socket_type::dish)
            , _req(_ctx, zmq::socket_type::dealer)
            , _rep(_ctx, zmq::socket_type::rep)
            , _poll_items{
                      {_rep, 0, ZMQ_POLLIN, 0},
                      {_sub, 0, ZMQ_POLLIN, 0},
              } {
        _rep.bind("tcp://*:" + std::to_string(_config.tcp_port));
        _sub.bind("udp://*:" + std::to_string(_config.udp_port));
    }

    void connect(const std::string& addr) {}

    void subscriptionHandler() { std::cout << "subscription " << std::endl; }

    void requestHandler() { std::cout << "request " << std::endl; }

    void beaconHandler() { std::cout << "beacon " << std::endl; }

    void run() {
        _timers.add(_config.beacon_interval, [this](int) { beaconHandler(); });
        while (true) {
            _timers.execute();
            zmq::poll(_poll_items, std::chrono::milliseconds(_timers.timeout()));
            if (_poll_items[0].revents & ZMQ_POLLIN) {
                requestHandler();
            }
            if (_poll_items[1].revents & ZMQ_POLLIN) {
                subscriptionHandler();
            }
        }
    }

private:
    Config _config;
    zmq::context_t _ctx;
    zmq::socket_t _pub;
    zmq::socket_t _sub;
    zmq::socket_t _req;
    zmq::socket_t _rep;
    zmq::timers_t _timers;
    std::vector<zmq::pollitem_t> _poll_items;
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
