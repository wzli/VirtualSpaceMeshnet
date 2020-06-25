#include <catch2/catch.hpp>
#include <vsm/mesh_node.hpp>
#include <vsm/zmq_transport.hpp>
#include <vsm/graphviz.hpp>

#include <deque>
#include <iostream>

using namespace vsm;

TEST_CASE("Single World", "[ego_sphere]") {
#if 0
    auto entity_update_handler = [](EntityT* new_version, const EntityT* old_version,
                                         const NodeInfoT* source, msecs timestamp) {
        std::cout << "entity_update: ts " << timestamp.count() << " source "
                  << (source ? source->name : "null") << " old "
                  << (old_version ? old_version->name : "null") << " new "
                  << (new_version ? new_version->name : "null") << std::endl;
    };
#endif
    MeshNode::Config config{
            msecs(1000),  // peer update interval
            msecs(1000),  // entity expiry interval
            {
                    // ego sphere
                    10,       // timestamp_lookup_size
                    nullptr,  // entity_update_handler
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
                (void) time;
                (void) level;
                ++error_counts[error.what()];
#if 0
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
#if 0
        for (const auto& entity : entity_lookup) {
            std::cout << entity.first << std::endl;
        }
#endif
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
    for (size_t i = 100; i < 100 + config.ego_sphere.timestamp_lookup_size; ++i) {
        msg.get()->mutate_timestamp(i);
        REQUIRE(mesh_node.forwardEntityUpdates(fbb, msg.get()));
    }
    REQUIRE(error_counts.count("ENTITY_TIMESTAMPS_TRIMMED"));

#if 0
    for (const auto& error_count : error_counts) {
        std::cout << error_count.second << " : " << error_count.first << std::endl;
    }
#endif
}

TEST_CASE("4 corners", "[ego_sphere]") {
    auto make_config = [](int id, std::vector<float> coords) {
        return MeshNode::Config{
                msecs(1),     // peer update interval
                msecs(1000),  // entity expiry interval
                {},
                {
                        // peer_manager
                        "node" + std::to_string(id),                  // name
                        "udp://127.0.0.1:1151" + std::to_string(id),  // address
                        std::move(coords),                            // coordinates
                        2,                                            // connection_degree
                },
                std::make_shared<ZmqTransport>("udp://*:1151" + std::to_string(id)),  // transport
                std::make_shared<Logger>(),                                           // logger
        };
    };
    std::vector<MeshNode::Config> configs{
            make_config(0, {0, 0}),
            make_config(1, {0, 1}),
            make_config(2, {1, 0}),
            make_config(3, {1, 1}),
    };
    std::vector<std::unordered_map<std::string, int>> error_counts(configs.size());
    auto make_log_handler = [&error_counts](int i) {
        return [&error_counts, i](
                       msecs time, Logger::Level level, Error error, const void*, size_t) {
            (void) time;
            (void) level;
            ++error_counts[i][error.what()];
            if (error.type == PeerTracker::PEER_COORDINATES_MISSING) {
                return;
            }
#if 0
            std::cout << time.count() << " " << i << " lv: " << level << ", type: " << error.type
                      << ", code: " << error.code << ", msg: " << error.what() << std::endl;
#endif
        };
    };
    std::deque<MeshNode> mesh_nodes;
    for (size_t i = 0; i < configs.size(); ++i) {
        mesh_nodes.emplace_back(configs[i]);
        mesh_nodes.back().getPeerTracker().latchPeer("udp://127.0.0.1:11510", 1);
    }
    // wait for mesh establishment
    for (int i = 0; i < 30; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
    }
    // check mesh connection
    for (size_t i = 0; i < configs.size(); ++i) {
        REQUIRE(mesh_nodes[i].getConnectedPeers().size() ==
                configs[i].peer_tracker.connection_degree);
        configs[i].logger->addLogHandler(Logger::TRACE, make_log_handler(i));
    }

    // create entity message
    std::vector<EntityT> entities;
    // expect in range
    entities.emplace_back();
    entities.back().name = "a";
    entities.back().coordinates = {-1, -1};
    entities.back().filter = Filter::ALL;
    entities.back().expiry = 1000;
    entities.back().range = 10;

    // exchange messages
    REQUIRE(mesh_nodes[0].updateEntities(entities).get());
    for (int i = 0; i < 10; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
    }

    REQUIRE(error_counts[0]["ENTITY_UPDATES_SENT"] == 1);
    for (size_t i = 0; i < configs.size(); ++i) {
        REQUIRE(error_counts[i]["ENTITY_CREATED"] == 1);
        REQUIRE(error_counts[i]["ENTITY_UPDATES_FORWARDED"] == 1);
        REQUIRE(error_counts[i].count("ENTITY_UPDATES_RECEIVED") == !!i);
        error_counts[i].clear();
    }

    // test nearest filter
    entities.back().name = "b";
    entities.back().coordinates = {1, 0};
    entities.back().filter = Filter::NEAREST;
    entities.back().expiry = 1000;
    entities.back().range = 10;

    // exchange messages
    REQUIRE(mesh_nodes[0].updateEntities(entities).get());
    for (int i = 0; i < 10; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
    }

    REQUIRE(error_counts[0]["ENTITY_UPDATES_FORWARDED"] == 1);
    REQUIRE(error_counts[1]["ENTITY_UPDATES_FORWARDED"] == 1);
    REQUIRE(error_counts[2]["ENTITY_NEAREST_FILTERED"] == 1);
    REQUIRE(error_counts[3]["ENTITY_NEAREST_FILTERED"] == 1);

#if 0
    // print error codes in order of frequency
    for (size_t i = 0; i < configs.size(); ++i) {
        printf("\r\nErrors for %zu\r\n", i);
        using Itr = std::unordered_map<std::string, int>::iterator;
        std::vector<Itr> iterators;
        for (auto it = error_counts[i].begin(); it != error_counts[i].end(); ++it) {
            iterators.emplace_back(it);
        }
        std::sort(iterators.begin(), iterators.end(), [](const Itr& a, const Itr& b) {
            // return a->second == b->second ? a->first < b->first : a->second > b->second;
            return a->first < b->first;
        });
        for (const auto& it : iterators) {
            if (it->first.find("ENTITY") != std::string::npos) {
                std::cout << it->second << " : " << it->first << std::endl;
            }
        }
    }
#endif
}
