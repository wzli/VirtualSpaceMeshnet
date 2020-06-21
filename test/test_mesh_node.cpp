#include <catch2/catch.hpp>
#include <vsm/mesh_node.hpp>
#include <vsm/zmq_transport.hpp>
#include <vsm/graphviz.hpp>
#include <vsm/time_sync.hpp>

#include <deque>
#include <iostream>

using namespace vsm;

TEST_CASE("MeshNode Update Tick", "[mesh_node]") {
    PeerTracker::Config peer_tracker_config{
            "node_name",              // name
            "udp://127.0.0.1:11611",  // address
            {0, 0},                   // coordinates
    };
    MeshNode::Config mesh_node_config{
            msecs(1),                                         // peer update interval
            std::move(peer_tracker_config),                   // peer tracker
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
        config.logger->addLogHandler(Logger::ERROR,
                [&config](msecs time, Logger::Level level, Error error, const void*, size_t) {
                    std::cout << time.count() << " " << config.peer_tracker.name << " lv: " << level
                              << ", type: " << error.type << ", code: " << error.code
                              << ", msg: " << error.what() << std::endl;
                });
        mesh_nodes.emplace_back(config);
        if (previous_address) {
            mesh_nodes.back().getPeerTracker().latchPeer(previous_address, 1);
        }
        previous_address = config.peer_tracker.address.c_str();
    }
    for (int i = 0; i < 5; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
    }

    REQUIRE(mesh_nodes[0].getConnectedPeers().front() == configs[1].peer_tracker.address);
    REQUIRE(mesh_nodes[1].getConnectedPeers().front() == configs[0].peer_tracker.address);
    REQUIRE(mesh_nodes[1].getPeerTracker().getPeerRankings().front()->node_info.address ==
            configs[0].peer_tracker.address);
    REQUIRE(mesh_nodes[0].getPeerTracker().getPeerRankings().front()->node_info.address ==
            configs[1].peer_tracker.address);
}

TEST_CASE("MeshNode Graph", "[mesh_node]") {
    auto make_config = [](int id, std::vector<float> coords) {
        char buf[10];
        sprintf(buf, "%02d", id);
        std::string id_str = buf;
        return MeshNode::Config{
                msecs(1),  // peer update interval
                {
                        "node" + id_str,                 // name
                        "udp://127.0.0.1:115" + id_str,  // address
                        std::move(coords),               // coordinates
                        4,                               // connection_degree
                        200,                             // lookup size
                        0,                               // rank decay
                },
                std::make_shared<ZmqTransport>("udp://*:115" + id_str),  // transport
                std::make_shared<Logger>(),                              // logger
        };
    };
    std::vector<MeshNode::Config> configs;
    int N = 7;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            configs.emplace_back(make_config(N * i + j, {(float) j, (float) i}));
        }
    }
    configs.back().peer_tracker.connection_degree = configs.size();

    const char* previous_address;
    SECTION("Centralized Boostrap") { previous_address = nullptr; }
    SECTION("Daisy-Chain Boostrap") { previous_address = configs[0].peer_tracker.address.c_str(); }

    std::deque<MeshNode> mesh_nodes;
    for (auto& config : configs) {
#if 0
        config.logger->addLogHandler(
                Logger::TRACE, [&config](msecs time, Logger::Level level,
                                       Error error, const void*, size_t) {
                    if (error.type == PeerTracker::PEER_COORDINATES_MISSING) {
                        return;
                    }
                    std::cout << time.count() << " " << config.peer_tracker.name << " lv: " << level
                              << ", type: " << error.type << ", code: " << error.code
                              << ", msg: " << error.what() << std::endl;
                });
#endif
        mesh_nodes.emplace_back(config);

        if (previous_address) {
            mesh_nodes.back().getPeerTracker().latchPeer(previous_address, 1);
            previous_address = config.peer_tracker.address.c_str();
        } else {
            mesh_nodes.back().getPeerTracker().latchPeer(
                    configs.front().peer_tracker.address.c_str(), 1);
        }
    }

    for (auto& config : configs) {
        mesh_nodes.back().getPeerTracker().latchPeer(config.peer_tracker.address.c_str());
    }

    Graphviz graphviz;
    configs.back().logger->addLogHandler(Logger::TRACE,
            [&graphviz](msecs, Logger::Level, Error error, const void* data, size_t) {
                switch (error.type) {
                    case MeshNode::SOURCE_UPDATE_RECEIVED:
                        graphviz.receivePeerUpdates(fb::GetRoot<Message>(data));
                        break;
                    case PeerTracker::PEER_UPDATED:
                        NodeInfoT* node_info =
                                reinterpret_cast<NodeInfoT*>(const_cast<void*>(data));
                        node_info->coordinates.clear();
                        break;
                }
            });

    for (int i = 0; i < 60; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
// for printing time sync csv
#if 0
        for (auto& mesh_node : mesh_nodes) {
            std::cout << mesh_node.getTimeSync().getTime().count() << ",";
        }
        puts("");
#endif
// for generating graph
#if 1
        char buf[10];
        sprintf(buf, "%02d", i);
        graphviz.saveGraph(
                "test_graph_" + std::string(buf) + ".gv", configs.back().peer_tracker.address);
#endif
    }

    for (int i = 2; i < N - 2; ++i) {
        for (int j = 2; j < N - 2; ++j) {
            auto& mesh_node = mesh_nodes[N * i + j];
            auto& config = configs[N * i + j];
            auto peer_rankings = mesh_node.getPeerTracker().getPeerRankings();
            REQUIRE(peer_rankings.size() >= config.peer_tracker.connection_degree);
            for (size_t k = 0; k < config.peer_tracker.connection_degree; ++k) {
                REQUIRE(distanceSqr(peer_rankings[k]->node_info.coordinates,
                                mesh_node.getPeerTracker().getNodeInfo().coordinates) <= 1);
                REQUIRE(mesh_node.getConnectedPeers().size() ==
                        config.peer_tracker.connection_degree + 1);
            }
        }
    }

// for printing node connections
#if 0
    for (int i = 0; i < mesh_nodes.size(); ++i) {
        printf("%02d: ", i);
        for (const auto& peer : mesh_nodes[i].getConnectedPeers()) {
            printf("%s ", peer.c_str() + 19);
        }
        printf("\n    ");
        for (const auto& peer : mesh_nodes[i].getPeerTracker().getPeerRankings()) {
            printf("%s ", peer->node_info.name.c_str() + 4);
        }
        puts("");
    }
#endif
}
