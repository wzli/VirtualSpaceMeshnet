#include <catch2/catch.hpp>
#include <vsm/zmq_transport.hpp>

#include <iostream>

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
    rx_socket.set(zmq::sockopt::rcvtimeo, 1);
    REQUIRE(zmq_recvmsg(rx_socket.handle(), rx_msg.handle(), 0) > 0);
}

#if 0
TEST_CASE("zmq transport inproc") {
    ZmqTransport zmq_transport("udp://*:11511");

    SECTION("register receiver handler") {
        zmq_transport.addReceiver([](const std::string& src_addr, const void* buffer, size_t len){
            std::cout << "got something\n";
        });
    }

    SECTION("make loopback connection") {
        REQUIRE(zmq_transport.connect("udp://127.0.0.1:11511") == 0);
    }

    SECTION("send test message") {
        std::string test_msg = "Hello!";
        REQUIRE(zmq_transport.transmit(test_msg.c_str(), test_msg.size()) == test_msg.size());
    }

    SECTION("poll timeout") {
        int error = zmq_transport.poll(50);
        std::cout << zmq_strerror(error) << std::endl;
        #if 0
        REQUIRE(zmq_transport.poll(0) == 0);
        REQUIRE(zmq_transport.poll(1) == EAGAIN);
        #endif
    }
}
#endif
