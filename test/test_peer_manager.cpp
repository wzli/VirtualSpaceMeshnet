#include <catch2/catch.hpp>
#include <vsm/zmq_transport.hpp>
#include <vsm/peer_manager.hpp>
#include <vsm/mesh_node.hpp>

#include <iostream>

using namespace vsm;
using namespace flatbuffers;

TEST_CASE("beacon tick") {
    auto logger = std::make_shared<Logger>();
    int beacon_count = 0;
    logger->addLogHandler(
            Logger::TRACE, [&beacon_count](Logger::Level, Error, const void*, size_t) {
                // std::cout << "lv: " << level << ", type: " << error.type << ", code: " <<
                // error.code
                //          << ", msg: " << error.what() << std::endl;
                ++beacon_count;
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
    REQUIRE(beacon_count >= 5);
}

TEST_CASE("node info serialization") {
    // serialize NodeInfo Msg
    FlatBufferBuilder fbb;
    auto peer_name = fbb.CreateString("peer_name");
    auto peer_addr = fbb.CreateString("peer_addr");
    Vector2 peer_coords(3, 4);
    auto node_info_offset = CreateNodeInfo(fbb, peer_name, peer_addr, &peer_coords, 100);
    fbb.Finish(node_info_offset);

    // deserialize
    auto node_info = GetRoot<NodeInfo>(fbb.GetBufferPointer());

    // verify schema
    Verifier verifier(fbb.GetBufferPointer(), fbb.GetSize());
    REQUIRE(node_info->Verify(verifier));
#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
    REQUIRE(verifier.GetComputedSize() == fbb.GetSize());
#endif

    // verity data
    REQUIRE(node_info->name()->str() == "peer_name");
    REQUIRE(node_info->address()->str() == "peer_addr");
    REQUIRE(node_info->coordinates()->x() == peer_coords.x());
    REQUIRE(node_info->coordinates()->y() == peer_coords.y());
    REQUIRE(node_info->timestamp() == 100);

    PeerManager::Config config{
            "name",     // name
            "address",  // address
            {0, 0},     // coordinates
            nullptr,    // logger
    };
    PeerManager peer_manager(config);
    REQUIRE(peer_manager.updatePeer(node_info, verifier.GetComputedSize()));
    REQUIRE(peer_manager.getPeers().size() == 1);
    REQUIRE(peer_manager.getPeers().at("peer_addr").node_info.name == "peer_name");
}

TEST_CASE("Peer Ranking") {
    auto logger = std::make_shared<Logger>();
    logger->addLogHandler(Logger::TRACE, [](Logger::Level level, Error error, const void*, size_t) {
        std::cout << "lv: " << level << ", type: " << error.type << ", code: " << error.code
                  << ", msg: " << error.what() << std::endl;
    });
    PeerManager::Config config{
            "my_name",     // name
            "my_address",  // address
            {0, 0},        // coordinates
            logger,        // logger
            7,             // connection_degree
            5,             // latch duration
            10,            // lookup size
            0.001,         // rank decay
    };
    PeerManager peer_manager(config);

    FlatBufferBuilder fbb;

    for (int i = 9; i >= 0; --i) {
        NodeInfoT peer;
        peer.name = "peer" + std::to_string(i);
        peer.address = "address" + std::to_string(i);
        peer.coordinates = std::make_unique<Vector2>(i, i);
        peer.timestamp = i;
        fbb.Finish(NodeInfo::Pack(fbb, &peer));
        peer_manager.updatePeer(GetRoot<NodeInfo>(fbb.GetBufferPointer()), fbb.GetSize());
    }
    for (int i = 3; i < 6; ++i) {
        peer_manager.latchPeer("address" + std::to_string(i), 2);
    }
    REQUIRE(peer_manager.getPeers().size() == 10);
    for (auto& peer : peer_manager.getPeers()) {
        std::cout << peer.first << " name " << peer.second.node_info.name << std::endl;
    }

    fbb.Clear();
    std::vector<std::string> recipients;
    auto ranked_peers = peer_manager.updatePeerRankings(fbb, recipients, 1);
    for (const auto& recipient : recipients) {
        std::cout << " recipient " << recipient << std::endl;
    }
    fbb.Finish(fbb.CreateVector(ranked_peers));
    auto rankings = GetRoot<Vector<Offset<NodeInfo>>>(fbb.GetBufferPointer());

    for (const auto& ranked : *rankings) {
        std::cout << " ranked " << ranked->name()->str() << std::endl;
    }
}
