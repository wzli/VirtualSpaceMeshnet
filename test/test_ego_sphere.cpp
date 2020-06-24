#include <catch2/catch.hpp>
#include <vsm/mesh_node.hpp>
#include <vsm/zmq_transport.hpp>
#include <vsm/graphviz.hpp>

#include <deque>
#include <iostream>

using namespace vsm;

TEST_CASE("Ego Sphere", "[ego_sphere]") {
    auto make_config = [](int id, std::vector<float> coords) {
        char buf[10];
        sprintf(buf, "%02d", id);
        std::string id_str = buf;
        return MeshNode::Config{
                msecs(1),     // peer update interval
                msecs(1000),  // entity expiry interval
                {},           // ego sphere
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
    configs.emplace_back(make_config(0, {0, 0}));

    std::deque<MeshNode> mesh_nodes;
    for (auto& config : configs) {
#if 1
        config.logger->addLogHandler(Logger::TRACE,
                [&config](msecs time, Logger::Level level, Error error, const void*, size_t) {
                    if (error.type == PeerTracker::PEER_COORDINATES_MISSING) {
                        return;
                    }
                    std::cout << time.count() << " " << config.peer_tracker.name << " lv: " << level
                              << ", type: " << error.type << ", code: " << error.code
                              << ", msg: " << error.what() << std::endl;
                });
#endif
        mesh_nodes.emplace_back(config);
    }

    std::vector<EntityT> entity_updates;
    EntityT entity;
    entity.name = "a";
    entity_updates.emplace_back(entity);

    mesh_nodes[0].updateEntities(entity_updates);
}
