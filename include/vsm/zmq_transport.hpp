#pragma once
#include <vsm/transport.hpp>

#include <zmq.hpp>
#include <zmq_timers.hpp>

#include <unordered_map>

namespace vsm {

class ZmqTransport : public Transport {
public:
    // common interface
    int connect(const std::string& dst_addr) override;
    int disconnect(const std::string& dst_addr) override;

    int transmit(const void* buffer, size_t len, const std::string& group = "") override;

    int addReceiver(ReceiverCallback receiver_callback, const std::string& group = "") override;
    int addTimer(size_t interval, TimerCallback timer_callback) override;

    // implementation specific
    ZmqTransport(const std::string& addr = "udp://*:11511");
    int poll(int timeout);  // miliseconds, -1 = inf, 0 = non-blocking

    const zmq::socket_t& getTxSocket() const { return _tx_socket; }
    const zmq::socket_t& getRxSocket() const { return _rx_socket; }

private:
    zmq::context_t _zmq_ctx;
    zmq::socket_t _tx_socket;
    zmq::socket_t _rx_socket;
    zmq::timers_t _timers;
    zmq::message_t _rx_message;
    std::unordered_map<std::string, ReceiverCallback> _receiver_callbacks;
};

}  // namespace vsm
