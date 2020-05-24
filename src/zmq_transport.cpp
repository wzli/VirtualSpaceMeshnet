#include <vsm/zmq_transport.hpp>

namespace vsm {

ZmqTransport::ZmqTransport(const std::string& addr)
        : _tx_socket(_zmq_ctx, zmq::socket_type::radio)
        , _rx_socket(_zmq_ctx, zmq::socket_type::dish) {
    _rx_socket.bind(addr);
}

int ZmqTransport::connect(const std::string& dst_addr) {
    return zmq_connect(_tx_socket.handle(), dst_addr.c_str());
}

int ZmqTransport::disconnect(const std::string& dst_addr) {
    return zmq_disconnect(_tx_socket.handle(), dst_addr.c_str());
}

int ZmqTransport::transmit(const void* buffer, size_t len, const std::string& group) {
    zmq::message_t msg(buffer, len);
    msg.set_group(group.c_str());
    return zmq_sendmsg(_tx_socket.handle(), msg.handle(), 0);
}

int ZmqTransport::addReceiver(ReceiverCallback receiver_callback, const std::string& group) {
    _receiver_callbacks[group] = receiver_callback;
    return zmq_join(_rx_socket.handle(), group.c_str());
}

int ZmqTransport::addTimer(size_t interval, TimerCallback timer_callback) {
    return _timers.add(interval, timer_callback);
}

int ZmqTransport::poll(int timeout) {
    _timers.execute();
    timeout = std::min<int>(timeout, _timers.timeout() & 0x7FFFFFFF);
    _rx_socket.set(zmq::sockopt::rcvtimeo, timeout);
    int n_msgs = 0;
    while (zmq_recvmsg(_rx_socket.handle(), _rx_message.handle(), n_msgs ? ZMQ_DONTWAIT : 0) > 0) {
        ++n_msgs;
        auto receiver_callback = _receiver_callbacks.find(_rx_message.group());
        if (receiver_callback != _receiver_callbacks.end()) {
            receiver_callback->second(_rx_message.data(), _rx_message.size());
        }
    }
    return n_msgs ? 0 : zmq_errno();
}

}  // namespace vsm
