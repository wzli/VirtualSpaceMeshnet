#include <catch2/catch.hpp>
#include <vsm/peer_tracker.hpp>
#include <random>
#include <iostream>

using namespace vsm;
using namespace flatbuffers;

TEST_CASE("NodeInfo Serialization", "[flatbuffers][peer_tracker]") {
    // serialize NodeInfo Msg
    FlatBufferBuilder fbb;
    auto peer_name = fbb.CreateString("peer_name");
    auto peer_addr = fbb.CreateString("peer_addr");
    std::vector<float> peer_coords{3, 4};
    auto node_info_offset = CreateNodeInfo(
            fbb, peer_name, peer_addr, fbb.CreateVector(peer_coords), 0xFFFFFFFF, 100);
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
    REQUIRE(distanceSqr(*node_info->coordinates(), peer_coords) == 0);
    REQUIRE(node_info->sequence() == 100);

    PeerTracker peer_tracker({
            "name",     // name
            "address",  // address
            {0, 0},     // coordinates
    });
    REQUIRE(peer_tracker.updatePeer(node_info) == PeerTracker::SUCCESS);
    REQUIRE(peer_tracker.getPeers().size() == 1);
    REQUIRE(peer_tracker.getPeers().at("peer_addr").node_info.name == "peer_name");
}

TEST_CASE("Peer Ranking", "[peer_tracker]") {
    FlatBufferBuilder fbb;
    auto logger = std::make_shared<Logger>();
#if 0
    logger->addLogHandler(Logger::TRACE, [](msecs time, Logger::Level level, Error error, const void*, size_t) {
        std::cout << "t " << time.count() << "lv: " << level << ", type: " << error.type << ", code: " << error.code
                  << ", msg: " << error.what() << std::endl;
    });
#endif
    // configer and create peer tracker
    size_t n_peers = 10;
    int latch_start = 3;
    int latch_end = 6;
    PeerTracker::Config config{
            "my_name",     // name
            "my_address",  // address
            {0, 0},        // coordinates
                           // tracking duration
    };

    SECTION("1") {}

    SECTION("2") { latch_end = 7; }

    SECTION("3") {
        n_peers = 1000;
        latch_start = 300;
        latch_end = 600;
    }

    PeerTracker peer_tracker(config, logger);
    // setup random generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    // add peers
    for (size_t i = 0; i < n_peers; ++i) {
        NodeInfoT peer;
        peer.name = "peer" + std::to_string(i);
        peer.address = "address" + std::to_string(i);
        peer.coordinates = {dis(gen), dis(gen)};
        peer.sequence = i;
        fbb.Finish(NodeInfo::Pack(fbb, &peer));
        peer_tracker.updatePeer(GetRoot<NodeInfo>(fbb.GetBufferPointer()));
    }
    // latch some peers
    for (int i = latch_start; i < latch_end; ++i) {
        peer_tracker.latchPeer(("address" + std::to_string(i)).c_str());
    }
    REQUIRE(peer_tracker.getPeers().size() == n_peers);
    // generate peer rankings
    std::vector<std::string> selected_peers, recipients;
    peer_tracker.updatePeerSelections(selected_peers, recipients);  // tick internal sequence count
    peer_tracker.updatePeerSelections(selected_peers, recipients);
    // required latched peers to be in recipients list
    REQUIRE(selected_peers.size() == recipients.size());
    for (int i = latch_start; i < latch_end; ++i) {
        REQUIRE(recipients.end() !=
                std::find(recipients.begin(), recipients.end(), "address" + std::to_string(i)));
    }
}

TEST_CASE("Nearest Peer", "[peer_tracker]") {
    PeerTracker::Config config{
            "my_name",     // name
            "my_address",  // address
            {0, 0},        // coordinates
                           // tracking_duration
    };
    PeerTracker peer_tracker(config);
    // add peers
    FlatBufferBuilder fbb;
    for (size_t i = 1; i <= 10; ++i) {
        NodeInfoT peer;
        peer.name = std::to_string(i);
        peer.address = std::to_string(i);
        peer.coordinates = {(float) i, (float) i};
        peer.sequence = i;
        fbb.Finish(NodeInfo::Pack(fbb, &peer));
        REQUIRE(peer_tracker.updatePeer(GetRoot<NodeInfo>(fbb.GetBufferPointer())) ==
                PeerTracker::SUCCESS);
    }
    std::vector<std::string> pool;
    pool = {"3", "6", "8", "5"};
    REQUIRE(peer_tracker.nearestPeer(std::vector<float>{-4, -5}, pool).name == "my_name");
    REQUIRE(peer_tracker.nearestPeer(std::vector<float>{0, 0}, pool).name == "my_name");
    REQUIRE(peer_tracker.nearestPeer(std::vector<float>{2, 2}, pool).name == "3");
    REQUIRE(peer_tracker.nearestPeer(std::vector<float>{4, 5}, pool).name == "5");
    REQUIRE(peer_tracker.nearestPeer(std::vector<float>{10, 100}, pool).name == "8");
}
