#include <catch2/catch.hpp>
#include <vsm/peer_manager.hpp>
#include <iostream>

using namespace vsm;
using namespace flatbuffers;

TEST_CASE("NodeInfo Serialization", "[flatbuffers][peer_manager]") {
    // serialize NodeInfo Msg
    FlatBufferBuilder fbb;
    auto peer_name = fbb.CreateString("peer_name");
    auto peer_addr = fbb.CreateString("peer_addr");
    Vec2 peer_coords(3, 4);
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

    PeerManager peer_manager({
            "name",     // name
            "address",  // address
            {0, 0},     // coordinates
            nullptr,    // logger
    });
    REQUIRE(peer_manager.updatePeer(node_info) == PeerManager::SUCCESS);
    REQUIRE(peer_manager.getPeers().size() == 1);
    REQUIRE(peer_manager.getPeers().at("peer_addr").node_info.name == "peer_name");
}

TEST_CASE("Peer Panking", "[peer_manager]") {
    FlatBufferBuilder fbb;
    auto logger = std::make_shared<Logger>();
#if 0
    logger->addLogHandler(Logger::TRACE, [](Logger::Level level, Error error, const void*, size_t) {
        std::cout << "lv: " << level << ", type: " << error.type << ", code: " << error.code
                  << ", msg: " << error.what() << std::endl;
    });
#endif
    // configer and create peer manager
    size_t n_peers = 10;
    int latch_start = 3;
    int latch_end = 6;
    PeerManager::Config config{
            "my_name",     // name
            "my_address",  // address
            {0, 0},        // coordinates
            logger,        // logger
            msecs(5),      // latch duration
            7,             // connection_degree
            20,            // lookup size
            0.000,         // rank decay
    };

    SECTION("1") { config.connection_degree = 3; }

    SECTION("2") { config.connection_degree = 5; }

    SECTION("3") {
        latch_end = 7;
        config.connection_degree = 8;
    }

    SECTION("4") {
        n_peers = 1000;
        latch_start = 300;
        latch_end = 600;
        config.connection_degree = 400;
        config.lookup_size = 500;
    }

    PeerManager peer_manager(config);
    // add 10 peers
    for (size_t i = 0; i < n_peers; ++i) {
        NodeInfoT peer;
        peer.name = "peer" + std::to_string(i);
        peer.address = "address" + std::to_string(i);
        peer.coordinates = std::make_unique<Vec2>(i, i);
        peer.timestamp = i;
        fbb.Finish(NodeInfo::Pack(fbb, &peer));
        peer_manager.updatePeer(GetRoot<NodeInfo>(fbb.GetBufferPointer()));
    }
    // latch some peers
    for (int i = latch_start; i < latch_end; ++i) {
        peer_manager.latchPeer(("address" + std::to_string(i)).c_str(), msecs(2));
    }
    REQUIRE(peer_manager.getPeers().size() == n_peers);
    // generate peer rankings
    fbb.Clear();
    std::vector<std::string> recipients;
    auto ranked_peers = peer_manager.updatePeerRankings(fbb, recipients, msecs(1));
    fbb.Finish(fbb.CreateVector(ranked_peers));
    auto rankings = GetRoot<Vector<Offset<NodeInfo>>>(fbb.GetBufferPointer());
    // require that lookup size is not exceeded
    REQUIRE(peer_manager.getPeers().size() == std::min<size_t>(n_peers, config.lookup_size));
    // required latched peers to be in recipients list
    for (int i = latch_start; i < latch_end; ++i) {
        REQUIRE(recipients.end() !=
                std::find(recipients.begin(), recipients.end(), "address" + std::to_string(i)));
    }
    // require that ranked size matches connection degree
    REQUIRE(ranked_peers.size() == config.connection_degree);
    float last_distance_squared = 0;
    for (const auto& ranked : *rankings) {
        float distance_squared = distanceSqr(config.coordinates, *ranked->coordinates());
        REQUIRE(distance_squared >= last_distance_squared);
        last_distance_squared = distance_squared;
    }
}
