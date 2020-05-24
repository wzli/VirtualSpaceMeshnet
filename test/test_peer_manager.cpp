#include <catch2/catch.hpp>
#include <vsm/zmq_transport.hpp>
#include <vsm/peer_manager.hpp>

using namespace vsm;
namespace fbs = flatbuffers;

TEST_CASE("peer_manager") {
    // serialize NodeInfo Msg
    fbs::FlatBufferBuilder fbs_builder;
    auto peer_name = fbs_builder.CreateString("peer_name");
    auto peer_addr = fbs_builder.CreateString("peer_addr");
    Vector2 peer_coords(3, 4);
    auto node_info_offset = CreateNodeInfo(fbs_builder, peer_name, peer_addr, &peer_coords, 100);
    fbs_builder.Finish(node_info_offset);

    // deserialize
    auto node_info = fbs::GetRoot<NodeInfo>(fbs_builder.GetBufferPointer());

    // verify schema
    flatbuffers::Verifier verifier(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());
    REQUIRE(node_info->Verify(verifier));

    // verity data
    REQUIRE(node_info->name()->str() == "peer_name");
    REQUIRE(node_info->address()->str() == "peer_addr");
    REQUIRE(node_info->coordinates()->x() == peer_coords.x());
    REQUIRE(node_info->coordinates()->y() == peer_coords.y());
    REQUIRE(node_info->timestamp() == 100);

    PeerManager peer_manager;
    REQUIRE(peer_manager.updatePeer(node_info));
    REQUIRE(peer_manager.getPeers().size() == 1);
}
