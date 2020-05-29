#include <catch2/catch.hpp>
#include <vsm/mesh_node.hpp>
#include <vsm/zmq_transport.hpp>
#include <deque>
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
    std::vector<MeshNode::Config> configs{
            {
                    msecs(1),  // peer update interval
                    {
                            "node1",                  // name
                            "udp://127.0.0.1:11611",  // address
                            {0, 0},                   // coordinates
                            msecs(3),                 // latch duration
                            1,                        // connection_degree
                            20,                       // lookup size
                            0,                        // rank decay
                    },
                    std::make_shared<ZmqTransport>("udp://*:11611"),  // transport
                    std::make_shared<Logger>(),                       // logger
            },
            {
                    msecs(1),  // peer update interval
                    {
                            "node2",                  // name
                            "udp://127.0.0.1:11612",  // address
                            {1, 1},                   // coordinates
                            msecs(3),                 // latch duration
                            1,                        // connection_degree
                            20,                       // lookup size
                            0,                        // rank decay
                    },
                    std::make_shared<ZmqTransport>("udp://*:11612"),  // transport
                    std::make_shared<Logger>(),                       // logger
            }};

    std::deque<MeshNode> mesh_nodes;
    const char* previous_address = nullptr;
    for (auto& config : configs) {
        config.logger->addLogHandler(Logger::INFO,
                [&config](msecs time, Logger::Level level, Error error, const void*, size_t) {
                    std::cout << time.count() << " " << config.peer_manager.name << " lv: " << level
                              << ", type: " << error.type << ", code: " << error.code
                              << ", msg: " << error.what() << std::endl;
                });
        mesh_nodes.emplace_back(config);
        if (previous_address) {
            mesh_nodes.back().getPeerManager().latchPeer(previous_address, msecs(4));
        }
        previous_address = config.peer_manager.address.c_str();
    }
    for (int i = 0; i < 5; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
    }
}
