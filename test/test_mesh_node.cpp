#include <catch2/catch.hpp>
#include <vsm/mesh_node.hpp>
#include <vsm/zmq_transport.hpp>
#include <vsm/graphviz.hpp>
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
                            0,                        // connection_degree
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
    std::vector<const Peer*> ranked_peers;
    PeerManager::PeerRange peer_range;

    // node 1 doesn't latch node 2
    peer_range = mesh_nodes[0].getPeerManager().getLatchedPeers();
    REQUIRE(peer_range.begin == peer_range.end);
    // node 1 sends to node 2
    peer_range = mesh_nodes[0].getPeerManager().getRecipientPeers();
    REQUIRE(peer_range.begin + 1 == peer_range.end);
    REQUIRE((*peer_range.begin)->node_info.address == configs[1].peer_manager.address);
    // node 1 ranks node 2
    mesh_nodes[0].getPeerManager().getRankedPeers(ranked_peers);
    REQUIRE(ranked_peers.size() == 1);
    REQUIRE(ranked_peers.front()->node_info.address == configs[1].peer_manager.address);

    // node 2 latches node 1
    peer_range = mesh_nodes[1].getPeerManager().getLatchedPeers();
    REQUIRE((*peer_range.begin)->node_info.address == configs[0].peer_manager.address);
    // node 2 sends to node 1
    peer_range = mesh_nodes[1].getPeerManager().getRecipientPeers();
    REQUIRE(peer_range.begin + 1 == peer_range.end);
    REQUIRE((*peer_range.begin)->node_info.address == configs[0].peer_manager.address);
    // node 2 doesn't ranks node 1
    mesh_nodes[1].getPeerManager().getRankedPeers(ranked_peers);
    REQUIRE(ranked_peers.empty());

#if 0
    for (auto& mesh_node : mesh_nodes) {
        std::cout << mesh_node.getPeerManager().getNodeInfo().name << ":\n";
        for (auto peers = mesh_node.getPeerManager().getLatchedPeers(); peers.begin != peers.end;
                ++peers.begin) {
            std::cout << (*peers.begin)->node_info.address << " latched\n";
        }
        for (auto peers = mesh_node.getPeerManager().getRecipientPeers(); peers.begin != peers.end;
                ++peers.begin) {
            std::cout << (*peers.begin)->node_info.address << " recipient\n";
        }
        mesh_node.getPeerManager().getRankedPeers(ranked_peers);
        for (auto& peer : ranked_peers) {
            std::cout << peer->node_info.address << " ranked\n";
        }
    }
#endif
}

TEST_CASE("MeshNode Graph", "[mesh_node]") {
    auto make_config = [](int id, Vec2 coords) {
        char buf[10];
        sprintf(buf, "%02d", id);
        std::string id_str = buf;
        return MeshNode::Config{
                msecs(1),  // peer update interval
                {
                        "node" + id_str,                 // name
                        "udp://127.0.0.1:115" + id_str,  // address
                        coords,                          // coordinates
                        msecs(10),                       // latch duration
                        4,                               // connection_degree
                        100,                             // lookup size
                        0,                               // rank decay
                },
                std::make_shared<ZmqTransport>("udp://*:115" + id_str),  // transport
                std::make_shared<Logger>(),                              // logger
        };
    };
    std::vector<MeshNode::Config> configs;
    int N = 4;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            configs.emplace_back(make_config(N * j + i, Vec2(10 * i, 10 * j)));
        }
    }

    std::deque<MeshNode> mesh_nodes;
    const char* previous_address = nullptr;
    for (auto& config : configs) {
        config.logger->addLogHandler(Logger::ERROR,
                [&config](msecs time, Logger::Level level, Error error, const void*, size_t) {
                    std::cout << time.count() << " " << config.peer_manager.name << " lv: " << level
                              << ", type: " << error.type << ", code: " << error.code
                              << ", msg: " << error.what() << std::endl;
                });
        mesh_nodes.emplace_back(config);
#if 0
        mesh_nodes.back().getPeerManager().latchPeer(configs.front().peer_manager.address.c_str(), msecs(0));
#else
        if (previous_address) {
            mesh_nodes.back().getPeerManager().latchPeer(previous_address, msecs(4));
        }
        previous_address = config.peer_manager.address.c_str();
#endif
    }

    // configs.back().peer_manager.connection_degree = configs.size();
    for (auto& config : configs) {
        mesh_nodes.back().getPeerManager().latchPeer(
                config.peer_manager.address.c_str(), msecs(99999));
    }

    Graphviz graphviz;
    configs.back().logger->addLogHandler(Logger::TRACE,
            [&graphviz](msecs, Logger::Level, Error error, const void* data, size_t) {
                if (error.type == MeshNode::PEER_UPDATES_RECEIVED) {
                    graphviz.receivePeerUpdates(fb::GetRoot<Message>(data));
                }
            });
    for (int i = 0; i < 50; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
        graphviz.saveGraph(
                "test_graph" + std::to_string(i) + ".gv", configs.back().peer_manager.address);
        printf("tick %d\n", i);
    }
}
