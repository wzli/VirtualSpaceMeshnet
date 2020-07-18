#pragma once
#include <vsm/transport.hpp>
#include <vsm/zmq_timers.hpp>

#include <unordered_map>

namespace vsm {

class ZmqTransport : public Transport {
public:
    // common interface
    const std::string& getAddress() const { return _address; }

    int connect(const std::string& dst_addr) override {
        return zmq_connect(_tx_socket.handle(), dst_addr.c_str());
    }
    int disconnect(const std::string& dst_addr) override {
        return zmq_disconnect(_tx_socket.handle(), dst_addr.c_str());
    }

    int transmit(const void* buffer, size_t len, const std::string& group = "") override {
        zmq::message_t msg(buffer, len);
        msg.set_group(group.c_str());
        return zmq_sendmsg(_tx_socket.handle(), msg.handle(), 0);
    }

    int addReceiver(ReceiverCallback receiver_callback, const std::string& group = "") override {
        _receiver_callbacks[group] = receiver_callback;
        return zmq_join(_rx_socket.handle(), group.c_str());
    }

    int addTimer(std::chrono::milliseconds interval, TimerCallback timer_callback) override {
        return _timers.add(interval.count(), std::move(timer_callback));
    }

    int poll(std::chrono::milliseconds timeout);

    // implementation specific
    ZmqTransport(const std::string& address = "udp://*:11511");

    const zmq::socket_t& getTxSocket() const { return _tx_socket; }
    const zmq::socket_t& getRxSocket() const { return _rx_socket; }

private:
    std::string _address;
    zmq::context_t _zmq_ctx;
    zmq::socket_t _tx_socket;
    zmq::socket_t _rx_socket;
    zmq::message_t _rx_message;
    ZmqTimers _timers;
    std::unordered_map<std::string, ReceiverCallback> _receiver_callbacks;
};

}  // namespace vsm
