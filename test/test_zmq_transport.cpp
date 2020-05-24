#include <catch2/catch.hpp>
#include <vsm/zmq_transport.hpp>

using namespace vsm;

TEST_CASE("zmq raw radio-dish") {
    // create sockets
    zmq::context_t ctx;
    zmq::socket_t tx_socket(ctx, zmq::socket_type::radio);
    zmq::socket_t rx_socket(ctx, zmq::socket_type::dish);

    // test inputs
    std::string endpoint = "udp://127.0.0.1:11511";
    std::string test_msg = "hello";
    std::string group = "";

    // loopback connection
    rx_socket.bind(endpoint);
    tx_socket.connect(endpoint);
    REQUIRE(zmq_join(rx_socket.handle(), group.c_str()) == 0);

    // send message
    zmq::message_t tx_msg(test_msg.c_str(), test_msg.size());
    tx_msg.set_group(group.c_str());
    REQUIRE(zmq_sendmsg(tx_socket.handle(), tx_msg.handle(), 0) > 0);

    // receive message
    zmq::message_t rx_msg;
    rx_socket.set(zmq::sockopt::rcvtimeo, 100);
    REQUIRE(zmq_recvmsg(rx_socket.handle(), rx_msg.handle(), 0) > 0);
}

TEST_CASE("zmq transport") {
    // test inputs
    std::string endpoint = "udp://127.0.0.1:11511";
    std::string test_msg = "Hello!";

    // create loopback connection
    ZmqTransport zmq_transport(endpoint);
    REQUIRE(zmq_transport.connect(endpoint) == 0);

    // register receiver handler
    std::vector<std::string> rx_msgs;
    REQUIRE(zmq_transport.addReceiver([&rx_msgs](const void* buffer, size_t len) {
        rx_msgs.emplace_back(static_cast<const char*>(buffer), len);
    }) == 0);

    // send message
    REQUIRE(zmq_transport.transmit(test_msg.c_str(), test_msg.size()) ==
            static_cast<int>(test_msg.size()));
    REQUIRE(zmq_transport.transmit(test_msg.c_str(), test_msg.size()) ==
            static_cast<int>(test_msg.size()));

    // receive messages
    REQUIRE(zmq_transport.poll(100) == 0);
    REQUIRE(zmq_transport.poll(0) == EAGAIN);
    REQUIRE(rx_msgs.size() == 2);
    for (const auto& rx_msg : rx_msgs) {
        REQUIRE(test_msg == rx_msg);
    }
}
