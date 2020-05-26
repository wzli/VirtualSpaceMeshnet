#include <catch2/catch.hpp>
#include <vsm/zmq_transport.hpp>
#include <vsm/peer_manager.hpp>
#include <vsm/mesh_node.hpp>

#include <iostream>

using namespace vsm;
namespace fbs = flatbuffers;

TEST_CASE("mesh_node") {
    auto zmq_transport = std::unique_ptr<ZmqTransport>(new ZmqTransport("udp://127.0.0.1:11611"));
    MeshNode::Config mesh_node_config;
    mesh_node_config.log_level = Logger::TRACE;
    mesh_node_config.log_handler = [](Logger::Level level, Error error, const void*, size_t) {
        std::cout << "lv: " << level << ", type: " << error.type << ", code: " << error.code
                  << ", msg: " << error.what() << std::endl;
    };
    MeshNode mesh_node(std::move(mesh_node_config), std::move(zmq_transport));

    dynamic_cast<ZmqTransport*>(&mesh_node.getTransport())->poll(1100);
    dynamic_cast<ZmqTransport*>(&mesh_node.getTransport())->poll(1100);
    dynamic_cast<ZmqTransport*>(&mesh_node.getTransport())->poll(1100);
}

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
    fbs::Verifier verifier(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());
    REQUIRE(node_info->Verify(verifier));

    // verity data
    REQUIRE(node_info->name()->str() == "peer_name");
    REQUIRE(node_info->address()->str() == "peer_addr");
    REQUIRE(node_info->coordinates()->x() == peer_coords.x());
    REQUIRE(node_info->coordinates()->y() == peer_coords.y());
    REQUIRE(node_info->timestamp() == 100);

    PeerManager peer_manager("node_name", {0, 0});
    REQUIRE(peer_manager.updatePeer(node_info));
    REQUIRE(peer_manager.getPeers().size() == 1);
}
