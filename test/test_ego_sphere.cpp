#include <catch2/catch.hpp>
#include <vsm/mesh_node.hpp>
#include <vsm/zmq_transport.hpp>
#include <vsm/graphviz.hpp>

#include <deque>
#include <iostream>

using namespace vsm;

TEST_CASE("Single World", "[ego_sphere]") {
    auto make_config = [](int id, std::vector<float> coords) {
        char buf[10];
        sprintf(buf, "%02d", id);
        std::string id_str = buf;
        return MeshNode::Config{
                msecs(1000),  // peer update interval
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
    std::unordered_map<std::string, int> error_counts;
    for (auto& config : configs) {
#if 1
        config.logger->addLogHandler(
                Logger::TRACE, [&config, &error_counts](msecs time, Logger::Level level,
                                       Error error, const void*, size_t) {
                    ++error_counts[error.what()];

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

    std::vector<EntityT> entities;
    // expect expired
    entities.emplace_back();
    entities.back().name = "a";
    entities.back().expiry = 0;
    // expect not expired
    entities.emplace_back();
    entities.back().name = "b";
    entities.back().expiry = 1000;
    // expect missing coordinates
    entities.emplace_back();
    entities.back().name = "c";
    entities.back().expiry = 1000;
    entities.back().range = 10;
    // expect out of range
    entities.emplace_back();
    entities.back().name = "d";
    entities.back().expiry = 1000;
    entities.back().range = 10;
    entities.back().coordinates = {10, 1};
    // expect in range
    entities.emplace_back();
    entities.back().name = "e";
    entities.back().expiry = 1000;
    entities.back().range = 10;
    entities.back().coordinates = {9, 0};

    auto msg = mesh_nodes[0].updateEntities(entities);
    // send update again and expect to be rejected based on timestamp
    fb::FlatBufferBuilder fbb;
    REQUIRE(!mesh_nodes[0].forwardEntityUpdates(fbb, msg.get()));
    REQUIRE(error_counts.count("ENTITY_ALREADY_RECEIVED"));

    for (const auto& error_count : error_counts) {
        std::cout << error_count.second << " : " << error_count.first << std::endl;
    }

    mesh_nodes[0].readEntities([](const EgoSphere::EntityLookup& entity_lookup) {
        for (const auto& entity : entity_lookup) {
            std::cout << entity.first << std::endl;
        }
    });

    REQUIRE(msg.get());
    REQUIRE(msg.get()->entities());
    REQUIRE(msg.get()->entities()->size() == 2);
    REQUIRE(error_counts.at("ENTITY_CREATED") == 2);

    REQUIRE(!msg.get()->entities()->LookupByKey("a"));
    REQUIRE(error_counts.count("Received ENTITY_EXPIRED"));
    REQUIRE(msg.get()->entities()->LookupByKey("b"));
    REQUIRE(!msg.get()->entities()->LookupByKey("c"));
    REQUIRE(error_counts.count("ENTITY_COORDINATES_MISSING"));
    REQUIRE(!msg.get()->entities()->LookupByKey("d"));
    REQUIRE(error_counts.count("ENTITY_RANGE_EXCEEDED"));
    REQUIRE(msg.get()->entities()->LookupByKey("e"));
}
