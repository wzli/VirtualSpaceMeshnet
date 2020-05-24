#pragma once
#include <vsm/transport.hpp>

#include <zmq.hpp>
#include <zmq_timers.hpp>

#include <unordered_map>

namespace vsm {

class ZmqTransport : public Transport {
public:
    ZmqTransport(const std::string& addr = "udp://*:11511");

    int connect(const std::string& dst_addr) override;
    int disconnect(const std::string& dst_addr) override;

    int transmit(const std::string& group, const uint8_t* buffer, size_t len) override;

    int addReceiver(const std::string& group, ReceiverCallback receiver_callback) override;
    int addTimer(size_t interval, TimerCallback timer_callback) override;

    int poll(int timeout);  // miliseconds, -1 = inf, 0 = non-blocking

private:
    zmq::context_t _zmq_ctx;
    zmq::socket_t _tx_socket;
    zmq::socket_t _rx_socket;
    zmq::timers_t _timers;
    zmq::message_t _rx_message;
    std::unordered_map<std::string, ReceiverCallback> _receiver_callbacks;
};

}  // namespace vsm
