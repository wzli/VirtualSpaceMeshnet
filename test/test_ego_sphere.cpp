#include <catch2/catch.hpp>
#include <vsm/mesh_node.hpp>
#include <vsm/zmq_transport.hpp>
#include <vsm/graphviz.hpp>

#include <deque>
#include <iostream>

using namespace vsm;

TEST_CASE("Single World", "[ego_sphere]") {
    auto entity_update_handler = [](EntityT* new_version, const EntityT* old_version,
                                         const NodeInfoT* source, msecs timestamp) {
        std::cout << "entity_update: ts " << timestamp.count() << " source "
                  << (source ? source->name : "null") << " old "
                  << (old_version ? old_version->name : "null") << " new "
                  << (new_version ? new_version->name : "null") << std::endl;
    };
    MeshNode::Config config{
            msecs(1000),  // peer update interval
            msecs(1000),  // entity expiry interval
            {
                    // ego sphere
                    10,                    // timestamp_lookup_size
                    entity_update_handler  // entity_update_handler
            },
            {
                    // peer_manager
                    "node",                   // name
                    "udp://127.0.0.1:11511",  // address
                    {0, 0},                   // coordinates
                    4,                        // connection_degree
                    200,                      // lookup size
                    0,                        // rank decay
            },
            std::make_shared<ZmqTransport>("udp://*:11511"),  // transport
            std::make_shared<Logger>(),                       // logger
    };

    std::unordered_map<std::string, int> error_counts;
    config.logger->addLogHandler(
            Logger::TRACE, [&config, &error_counts](msecs time, Logger::Level level, Error error,
                                   const void*, size_t) {
                ++error_counts[error.what()];
#if 1
                if (error.type == PeerTracker::PEER_COORDINATES_MISSING) {
                    return;
                }
                std::cout << time.count() << " " << config.peer_tracker.name << " lv: " << level
                          << ", type: " << error.type << ", code: " << error.code
                          << ", msg: " << error.what() << std::endl;
#endif
            });
    MeshNode mesh_node(config);

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

    // trigger update after 1 ms

    mesh_node.getTransport().poll(msecs(1));
    auto msg = mesh_node.updateEntities(entities);

    // send update again and expect to be rejected based on repeated timestamp
    fb::FlatBufferBuilder fbb;
    REQUIRE(!mesh_node.forwardEntityUpdates(fbb, msg.get()));
    REQUIRE(error_counts.count("ENTITY_ALREADY_RECEIVED"));
    // expect message forwarded to contain only "b" and "e"
    REQUIRE(msg.get());
    REQUIRE(msg.get()->entities());
    REQUIRE(msg.get()->entities()->size() == 2);
    REQUIRE(error_counts.at("ENTITY_CREATED") == 2);
    mesh_node.readEntities([](const EgoSphere::EntityLookup& entity_lookup) {
        REQUIRE(entity_lookup.size() == 2);
        REQUIRE(entity_lookup.count("b"));
        REQUIRE(entity_lookup.count("e"));
        for (const auto& entity : entity_lookup) {
            std::cout << entity.first << std::endl;
        }
    });

    // check returned message for expected entity rejections
    REQUIRE(!msg.get()->entities()->LookupByKey("a"));
    REQUIRE(error_counts.count("Received ENTITY_EXPIRED"));
    REQUIRE(msg.get()->entities()->LookupByKey("b"));
    REQUIRE(!msg.get()->entities()->LookupByKey("c"));
    REQUIRE(error_counts.count("ENTITY_COORDINATES_MISSING"));
    REQUIRE(!msg.get()->entities()->LookupByKey("d"));
    REQUIRE(error_counts.count("ENTITY_RANGE_EXCEEDED"));
    REQUIRE(msg.get()->entities()->LookupByKey("e"));

    // test delete
    REQUIRE(!mesh_node.getEgoSphere().deleteEntity("a", msecs(0)));
    REQUIRE(mesh_node.getEgoSphere().deleteEntity("b", msecs(0)));

    // test expire
    REQUIRE(error_counts.count("ENTITY_DELETED"));
    mesh_node.getEgoSphere().expireEntities(msecs(2000));
    REQUIRE(error_counts.count("ENTITY_EXPIRED"));

    // test timestamp lookup trimming
    REQUIRE(!error_counts.count("ENTITY_TIMESTAMPS_TRIMMED"));
    for (size_t i = 2; i < 2 + config.ego_sphere.timestamp_lookup_size; ++i) {
        msg.get()->mutate_timestamp(i);
        REQUIRE(mesh_node.forwardEntityUpdates(fbb, msg.get()));
    }
    REQUIRE(error_counts.count("ENTITY_TIMESTAMPS_TRIMMED"));

    for (const auto& error_count : error_counts) {
        std::cout << error_count.second << " : " << error_count.first << std::endl;
    }
}
