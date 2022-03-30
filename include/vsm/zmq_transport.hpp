#pragma once
#include <vsm/transport.hpp>
#include <vsm/zmq_timers.hpp>

#include <string>
#include <unordered_map>

namespace vsm {

class ZmqTransport : public Transport {
public:
    // common interface
    const char* getAddress() const { return _address.c_str(); }

    int connect(const char* dst_addr) override {
        return zmq_connect(_tx_socket.handle(), dst_addr);
    }
    int disconnect(const char* dst_addr) override {
        return zmq_disconnect(_tx_socket.handle(), dst_addr);
    }

    int transmit(const void* buffer, size_t len, const char* group = "") override {
        zmq::message_t msg(buffer, len);
        msg.set_group(group);
        return zmq_sendmsg(_tx_socket.handle(), msg.handle(), 0);
    }

    int addReceiver(ReceiverCallback receiver_callback, const char* group = "") override {
        _receiver_callbacks[group] = receiver_callback;
        return zmq_join(_rx_socket.handle(), group);
    }

    int addTimer(size_t interval_ms, TimerCallback timer_callback) override {
        return _timers.add(interval_ms, std::move(timer_callback));
    }

    int poll(size_t timeout_ms);

    // implementation specific
    ZmqTransport(std::string address = "udp://*:11511");

    const zmq::socket_t& getTxSocket() const { return _tx_socket; }
    const zmq::socket_t& getRxSocket() const { return _rx_socket; }
    zmq::context_t& getContext() { return _zmq_ctx; }

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
