#include <catch2/catch.hpp>
#include <vsm/mesh_node.hpp>
#include <vsm/zmq_transport.hpp>
#include <iostream>

using namespace vsm;

TEST_CASE("MeshNode Update Tick", "[mesh_node]") {
    auto logger = std::make_shared<Logger>();
    int peer_updates_sent = 0;
    logger->addLogHandler(
            Logger::TRACE, [&peer_updates_sent](Logger::Level, Error error, const void*, size_t) {
                peer_updates_sent += error.type == MeshNode::PEER_UPDATES_SENT;
                // std::cout << "lv: " << level << ", type: " << error.type << ", code: " <<
                // error.code << ", msg: " << error.what() << std::endl;
            });
    PeerManager::Config peer_manager_config{
            "node_name",              // name
            "udp://127.0.0.1:11611",  // address
            {0, 0},                   // coordinates
            logger,                   // logger
    };
    MeshNode::Config mesh_node_config{
            1,                                                // beacon_interval
            std::move(peer_manager_config),                   // peer_manager
            std::make_shared<ZmqTransport>("udp://*:11611"),  // transport
            logger,                                           // logger
    };
    MeshNode mesh_node(std::move(mesh_node_config));
    for (int i = 0; i < 5; ++i) {
        mesh_node.getTransport().poll(2);
    }
    REQUIRE(peer_updates_sent >= 4);
}

TEST_CASE("MeshNode Loopback", "[mesh_node]") {
    auto logger1 = std::make_shared<Logger>();
    auto logger2 = std::make_shared<Logger>();
    logger1->addLogHandler(
            Logger::TRACE, [](Logger::Level level, Error error, const void*, size_t) {
                std::cout << "1 - lv: " << level << ", type: " << error.type
                          << ", code: " << error.code << ", msg: " << error.what() << std::endl;
            });
    logger2->addLogHandler(
            Logger::TRACE, [](Logger::Level level, Error error, const void*, size_t) {
                std::cout << "2 - lv: " << level << ", type: " << error.type
                          << ", code: " << error.code << ", msg: " << error.what() << std::endl;
            });

    PeerManager::Config pm_config_1{
            "node1",                  // name
            "udp://127.0.0.1:11611",  // address
            {0, 0},                   // coordinates
            logger1,                  // logger
            1,                        // connection_degree
            20,                       // latch duration
            20,                       // lookup size
            0,                        // rank decay
    };

    PeerManager::Config pm_config_2{
            "node2",                  // name
            "udp://127.0.0.1:11612",  // address
            {1, 1},                   // coordinates
            logger2,                  // logger
            1,                        // connection_degree
            20,                       // latch duration
            20,                       // lookup size
            0,                        // rank decay
    };

    MeshNode::Config mn_config_1{
            1,                                                // beacon_interval
            pm_config_1,                                      // peer_manager
            std::make_shared<ZmqTransport>("udp://*:11611"),  // transport
            logger1,                                          // logger
    };

    MeshNode::Config mn_config_2{
            1,                                                // beacon_interval
            pm_config_2,                                      // peer_manager
            std::make_shared<ZmqTransport>("udp://*:11612"),  // transport
            logger2,                                          // logger
    };

    MeshNode mesh_node_1(mn_config_1);
    MeshNode mesh_node_2(mn_config_2);
    mesh_node_1.getPeerManager().latchPeer(pm_config_2.address.c_str(), 0);
    for (int i = 0; i < 5; ++i) {
        mesh_node_1.getTransport().poll(1);
        mesh_node_2.getTransport().poll(1);
    }
}
