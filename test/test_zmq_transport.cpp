#include <catch2/catch.hpp>
#include <vsm/zmq_transport.hpp>

using namespace vsm;

TEST_CASE("zmq transport") {
    ZmqTransport zmq_transport;

    SECTION("poll timeout") {
        zmq_transport.poll(0);
        zmq_transport.poll(10000);
        zmq_transport.poll(0);
        zmq_transport.poll(1000);
    }
}
