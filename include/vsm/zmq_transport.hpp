#pragma once
#include <vsm/transport.hpp>
#include <zmq.hpp>

#include <functional>
#include <unordered_map>
#include <vector>

namespace vsm {

class ZmqTimers {
public:
    ZmqTimers()
            : _timers(zmq_timers_new()) {}

    ~ZmqTimers() { zmq_timers_destroy(&_timers); }

    int add(size_t interval, std::function<void(int)> handler) {
        _callbacks.emplace_back(std::move(handler));
        return zmq_timers_add(_timers, interval, callback_wrapper, this);
    }

    int cancel(int timer_id) { return zmq_timers_cancel(_timers, timer_id); }

    int set_interval(int timer_id, size_t interval) {
        return zmq_timers_set_interval(_timers, timer_id, interval);
    }

    int reset(int timer_id) { return zmq_timers_reset(_timers, timer_id); }

    long timeout() { return zmq_timers_timeout(_timers); }

    int execute() { return zmq_timers_execute(_timers); }

private:
    static void callback_wrapper(int timer_id, void* arg) {
        reinterpret_cast<ZmqTimers*>(arg)->_callbacks[timer_id - 1](timer_id);
    }
    std::vector<std::function<void(int)>> _callbacks;
    void* _timers;
};

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
