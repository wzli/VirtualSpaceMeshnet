#include <vsm/mesh_node.hpp>

namespace vsm {

MeshNode::MeshNode(const Config& config, std::unique_ptr<Transport> transport)
        : _config(config)
        , _transport(std::move(transport))
        , _peer_manager() {}

}  // namespace vsm
