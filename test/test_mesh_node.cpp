#include <catch2/catch.hpp>
#include <vsm/mesh_node.hpp>
#include <vsm/zmq_transport.hpp>
#include <iostream>

using namespace vsm;

TEST_CASE("MeshNode Update Tick", "[mesh_node]") {
    PeerManager::Config peer_manager_config{
            "node_name",              // name
            "udp://127.0.0.1:11611",  // address
            {0, 0},                   // coordinates
    };
    MeshNode::Config mesh_node_config{
            msecs(1),                                         // peer update interval
            std::move(peer_manager_config),                   // peer manager
            std::make_shared<ZmqTransport>("udp://*:11611"),  // transport
            std::make_shared<Logger>(),                       // logger
    };
    int peer_updates_sent = 0;
    mesh_node_config.logger->addLogHandler(Logger::TRACE,
            [&peer_updates_sent](msecs, Logger::Level, Error error, const void*, size_t) {
                peer_updates_sent += error.type == MeshNode::PEER_UPDATES_SENT;
                // std::cout << "lv: " << level << ", type: " << error.type << ", code: " <<
                // error.code << ", msg: " << error.what() << std::endl;
            });
    MeshNode mesh_node(std::move(mesh_node_config));
    for (int i = 0; i < 5; ++i) {
        mesh_node.getTransport().poll(msecs(2));
    }
    REQUIRE(peer_updates_sent >= 4);
}

TEST_CASE("MeshNode Loopback", "[mesh_node]") {
    PeerManager::Config pm_config_1{
            "node1",                  // name
            "udp://127.0.0.1:11611",  // address
            {0, 0},                   // coordinates
            msecs(20),                // latch duration
            1,                        // connection_degree
            20,                       // lookup size
            0,                        // rank decay
    };

    PeerManager::Config pm_config_2{
            "node2",                  // name
            "udp://127.0.0.1:11612",  // address
            {1, 1},                   // coordinates
            msecs(20),                // latch duration
            1,                        // connection_degree
            20,                       // lookup size
            0,                        // rank decay
    };

    MeshNode::Config mn_config_1{
            msecs(1),                                         // peer update interval
            pm_config_1,                                      // peer manager
            std::make_shared<ZmqTransport>("udp://*:11611"),  // transport
            std::make_shared<Logger>(),                       // logger
    };

    MeshNode::Config mn_config_2{
            msecs(1),                                         // peer update interval
            pm_config_2,                                      // peer manager
            std::make_shared<ZmqTransport>("udp://*:11612"),  // transport
            std::make_shared<Logger>(),                       // logger
    };

    mn_config_1.logger->addLogHandler(
            Logger::TRACE, [](msecs time, Logger::Level level, Error error, const void*, size_t) {
                std::cout << time.count() << " 1 - lv: " << level << ", type: " << error.type
                          << ", code: " << error.code << ", msg: " << error.what() << std::endl;
            });

    mn_config_2.logger->addLogHandler(
            Logger::TRACE, [](msecs time, Logger::Level level, Error error, const void*, size_t) {
                std::cout << time.count() << " 2 - lv: " << level << ", type: " << error.type
                          << ", code: " << error.code << ", msg: " << error.what() << std::endl;
            });

    MeshNode mesh_node_1(mn_config_1);
    MeshNode mesh_node_2(mn_config_2);
    mesh_node_1.getPeerManager().latchPeer(pm_config_2.address.c_str(), msecs(0));
    for (int i = 0; i < 5; ++i) {
        mesh_node_1.getTransport().poll(msecs(1));
        mesh_node_2.getTransport().poll(msecs(1));
    }
}
