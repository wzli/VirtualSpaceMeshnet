#include <catch2/catch.hpp>
#include <vsm/mesh_node.hpp>
#include <vsm/zmq_transport.hpp>
#include <vsm/graphviz.hpp>

#include <deque>
#include <iostream>

using namespace vsm;

static constexpr int64_t SECS = 1000000000;

TEST_CASE("Single World", "[ego_sphere]") {
#if 0
    auto entity_update_handler = [](EgoSphere::EntityUpdate* new_entity,
                                         const EgoSphere::EntityUpdate* old_entity,
                                         const NodeInfoT* source) {
        std::cout << "entity_update: source " << (source ? source->name : "null") << " old "
                  << (old_entity ? old_entity->entity.name : "null") << " new "
                  << (new_entity ? new_entity->entity.name : "null") << std::endl;
    };
#endif
    MeshNode::Config config{
            msecs(1000),  // peer update interval
            msecs(1000),  // entity expiry interval
            8000,         // entity updates size
            false,        // spectator
            {
                    // ego sphere
                    nullptr,  // entity update handler
                    10,       // timestamp lookup size
            },
            {
                    // peer manager
                    "node",                   // name
                    "udp://127.0.0.1:11511",  // address
                    {0, 0},                   // coordinates
                                              // tracking duration
            },
            std::make_shared<ZmqTransport>("udp://*:11511"),  // transport
            std::make_shared<Logger>(),                       // logger
    };

    std::unordered_map<std::string, int> error_counts;
    config.logger->addLogHandler(
            Logger::TRACE, [&config, &error_counts](int64_t time, Logger::Level level, Error error,
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
    entities.back().expiry = 1;
    // expect not expired
    entities.emplace_back();
    entities.back().name = "b";
    entities.back().expiry = 10 * SECS;
    // expect missing coordinates
    entities.emplace_back();
    entities.back().name = "c";
    entities.back().expiry = 10 * SECS;
    entities.back().range = 10;
    // expect out of range
    entities.emplace_back();
    entities.back().name = "d";
    entities.back().expiry = 10 * SECS;
    entities.back().range = 10;
    entities.back().coordinates = {10, 1};
    // expect in range
    entities.emplace_back();
    entities.back().name = "e";
    entities.back().expiry = 1 * SECS;
    entities.back().range = 10;
    entities.back().coordinates = {9, 0};

    // trigger update after 1 ms

    mesh_node.getTransport().poll(msecs(2));
    auto msgs = mesh_node.updateEntities(entities);
    REQUIRE(!msgs.empty());

    // send update again and expect to be rejected based on repeated timestamp
    fb::FlatBufferBuilder fbb;
    REQUIRE(!mesh_node.forwardEntityUpdates(fbb, msgs[0].get()));
    REQUIRE(error_counts.count("ENTITY_ALREADY_RECEIVED"));
    // expect message forwarded to contain only "b" and "e"
    REQUIRE(msgs[0].get()->entities());
    REQUIRE(msgs[0].get()->entities()->size() == 2);
    REQUIRE(error_counts.at("ENTITY_CREATED") == 2);

    {
        auto entity_lookup = mesh_node.getEntities();
        REQUIRE(entity_lookup.first.size() == 2);
        REQUIRE(entity_lookup.first.count("b"));
        REQUIRE(entity_lookup.first.count("e"));
#if 0
        for (const auto& entity : entity_lookup.first) {
            std::cout << entity.first << std::endl;
        }
#endif
    }

    // check returned message for expected entity rejections
    REQUIRE(!msgs[0].get()->entities()->LookupByKey("a"));
    REQUIRE(error_counts.count("Received ENTITY_EXPIRED"));
    REQUIRE(msgs[0].get()->entities()->LookupByKey("b"));
    REQUIRE(!msgs[0].get()->entities()->LookupByKey("c"));
    REQUIRE(error_counts.count("ENTITY_COORDINATES_MISSING"));
    REQUIRE(!msgs[0].get()->entities()->LookupByKey("d"));
    REQUIRE(error_counts.count("ENTITY_RANGE_EXCEEDED"));
    REQUIRE(msgs[0].get()->entities()->LookupByKey("e"));

    // test delete
    auto& self = mesh_node.getPeerTracker().getNodeInfo();
    REQUIRE(!mesh_node.getEgoSphere().deleteEntity("a", self));
    REQUIRE(mesh_node.getEgoSphere().deleteEntity("b", self));

    // test expire
    REQUIRE(error_counts.count("ENTITY_DELETED"));
    mesh_node.getEgoSphere().expireEntities(2 * SECS, self);
    REQUIRE(error_counts.count("ENTITY_EXPIRED"));

    // test timestamp lookup trimming
    REQUIRE(!error_counts.count("ENTITY_TIMESTAMPS_TRIMMED"));
    for (size_t i = 100; i < 100 + config.ego_sphere.timestamp_lookup_size; ++i) {
        msgs[0].get()->mutate_timestamp(i);
        REQUIRE(mesh_node.forwardEntityUpdates(fbb, msgs[0].get()));
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
                8000,         // entity updates size
                false,        // spectator
                {},
                {
                        // peer manager
                        "node" + std::to_string(id),                  // name
                        "udp://127.0.0.1:1151" + std::to_string(id),  // address
                        std::move(coords),                            // coordinates
                                                                      // tracking duration
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
                       int64_t time, Logger::Level level, Error error, const void*, size_t) {
            (void) time;
            (void) level;
            ++error_counts[i][error.what()];
            if (error.type == PeerTracker::PEER_COORDINATES_MISSING) {
                return;
            }
#if 0
            std::cout << time << " " << i << " lv: " << level << ", type: " << error.type
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
        // REQUIRE(mesh_nodes[i].getConnectedPeers().size() ==
        //        configs[i].peer_tracker.connection_degree);
        configs[i].logger->addLogHandler(Logger::TRACE, make_log_handler(i));
    }

    // create entity message
    std::vector<EntityT> entities;
    // expect in range
    entities.emplace_back();
    entities.back().name = "a";
    entities.back().coordinates = {-1, -1};
    entities.back().filter = Filter::ALL;
    entities.back().expiry = 10 * SECS;
    entities.back().range = 10;

    // exchange messages
    REQUIRE(!mesh_nodes[0].updateEntities(entities).empty());
    for (int i = 0; i < 30; ++i) {
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

    // test entity delete propagation
    entities.back().filter = Filter::NEAREST;
    entities.back().expiry = 0;

    mesh_nodes[0].offsetRelativeExpiry(entities);
    REQUIRE(!mesh_nodes[0].updateEntities(entities).empty());
    for (int i = 0; i < 30; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
    }

    for (size_t i = 0; i < configs.size(); ++i) {
        REQUIRE(error_counts[i].count("Received ENTITY_EXPIRED"));
        error_counts[i].clear();
    }

    // test nearest filter (closest to self)
    entities.back().name = "b";
    entities.back().coordinates = {0, 0};
    entities.back().filter = Filter::NEAREST;
    entities.back().expiry = 10 * SECS;
    entities.back().range = 10;

    // exchange messages
    REQUIRE(!mesh_nodes[0].updateEntities(entities).empty());
    for (int i = 0; i < 30; ++i) {
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

    // test nearest filter (closest to adjacent)
    entities.back().name = "c";
    entities.back().coordinates = {0, 1};
    entities.back().filter = Filter::NEAREST;
    entities.back().expiry = 10 * SECS;
    entities.back().range = 10;

    // exchange messages
    REQUIRE(!mesh_nodes[0].updateEntities(entities).empty());
    for (int i = 0; i < 30; ++i) {
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

    // send again and make sure it gets filtered the second time
    REQUIRE(!mesh_nodes[0].updateEntities(entities).empty());
    for (int i = 0; i < 30; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
    }
    REQUIRE(error_counts[0]["ENTITY_UPDATES_SENT"] == 1);
    for (size_t i = 0; i < configs.size(); ++i) {
        REQUIRE(error_counts[i].count("ENTITY_CREATED") == 0);
        REQUIRE(error_counts[i]["ENTITY_UPDATES_RECEIVED"] == !!i);
        REQUIRE(error_counts[i]["ENTITY_UPDATES_FORWARDED"] == !(i & 1));
        REQUIRE(error_counts[i]["ENTITY_NEAREST_FILTERED"] == !!(i & 1));
        error_counts[i].clear();
    }

    // test nearest filter (closest to opposite)
    entities.back().name = "d";
    entities.back().coordinates = {1, 1};
    entities.back().filter = Filter::NEAREST;
    entities.back().expiry = 10 * SECS;
    entities.back().range = 10;

    // exchange messages
    REQUIRE(!mesh_nodes[0].updateEntities(entities).empty());
    for (int i = 0; i < 30; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
    }

    REQUIRE(error_counts[1]["ENTITY_NEAREST_FILTERED"] == 1);
    REQUIRE(error_counts[2]["ENTITY_NEAREST_FILTERED"] == 1);
    REQUIRE(error_counts[3].count("ENTITY_UPDATES_RECEIVED") == 0);
    for (size_t i = 0; i < configs.size(); ++i) {
        error_counts[i].clear();
    }

    // test hop limit
    entities.back().name = "e";
    entities.back().hop_limit = 1;
    entities.back().filter = Filter::ALL;

    // exchange messages
    REQUIRE(!mesh_nodes[0].updateEntities(entities).empty());
    for (int i = 0; i < 30; ++i) {
        for (auto& mesh_node : mesh_nodes) {
            mesh_node.getTransport().poll(msecs(1));
        }
    }

    for (size_t i = 0; i < configs.size(); ++i) {
        REQUIRE(error_counts[i]["ENTITY_CREATED"] == (i < 3));
        error_counts[i].clear();
    }

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
