#include <vsm/mesh_node.hpp>

namespace vsm {

namespace fbs = flatbuffers;

MeshNode::MeshNode(const Config& config, std::unique_ptr<Transport> transport)
        : _config(config)
        , _transport(std::move(transport))
        , _peer_manager() {}

void MeshNode::addPeer(const std::string& peer_address) {
    fbs::FlatBufferBuilder fbs_builder;
    NodeInfoBuilder node_info_builder(fbs_builder);
    auto peer_addr = fbs_builder.CreateString(peer_address);
}

}  // namespace vsm
