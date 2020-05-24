#include <catch2/catch.hpp>
#include <vsm/zmq_transport.hpp>

#include <iostream>

using namespace vsm;

TEST_CASE("zmq transport inproc") {
    //ZmqTransport zmq_transport("udp://*:11511");
    ZmqTransport zmq_transport("inproc://test0");

    SECTION("register receiver handler") {
        zmq_transport.addReceiver([](const std::string& src_addr, const void* buffer, size_t len){
            std::cout << "got something\n";
        });
    }

    SECTION("make loopback connection") {
        REQUIRE(zmq_transport.connect("inproc://test0") == 0);
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
