#include <vsm/zmq_transport.hpp>
#include <iostream>

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

int ZmqTransport::transmit(const std::string& group, const uint8_t* buffer, size_t len) {
    zmq::message_t msg(buffer, len);
    msg.set_group(group.c_str());
    return zmq_sendmsg(_tx_socket.handle(), msg.handle(), 0);
}

int ZmqTransport::addReceiver(const std::string& group, ReceiverCallback receiver_callback) {
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
    int bytes_received = zmq_recvmsg(_rx_socket.handle(), _rx_message.handle(), 0);
    if (bytes_received > 0) {
        auto receiver_callback = _receiver_callbacks.find(_rx_message.group());
        if (receiver_callback != _receiver_callbacks.end()) {
            receiver_callback->second(_rx_message.gets("Peer-Address"),
                    static_cast<uint8_t*>(_rx_message.data()), _rx_message.size());
        }
    }
    return bytes_received;
}

}  // namespace vsm
