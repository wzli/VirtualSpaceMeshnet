#include <vsm/zmq_transport.hpp>

namespace vsm {

ZmqTransport::ZmqTransport(std::string address)
        : _address(std::move(address))
        , _tx_socket(_zmq_ctx, zmq::socket_type::radio)
        , _rx_socket(_zmq_ctx, zmq::socket_type::dish) {
    _rx_socket.bind(_address);
}

int ZmqTransport::poll(std::chrono::milliseconds timeout) {
    _timers.execute();
    int next_timeout = std::min<uint32_t>(timeout.count(), _timers.timeout());
    _rx_socket.set(zmq::sockopt::rcvtimeo, next_timeout);
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
