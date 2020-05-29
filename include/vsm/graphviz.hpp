#pragma once
#include <vsm/msg_types_generated.h>
#include <fstream>
#include <iostream>
#include <unordered_map>

namespace vsm {

class Graphviz {
public:
    struct Node {
        NodeInfoT info;
        std::vector<std::string> peers;
    };

    bool receivePeerUpdates(const Message* msg) {
        if (!msg || !msg->peers() || !msg->source() || !msg->source()->address()) {
            return false;
        }
        auto& node = _nodes[msg->source()->address()->c_str()];
        if (msg->source()->timestamp() < node.info.timestamp) {
            return false;
        }
        msg->source()->UnPackTo(&(node.info));
        node.peers.clear();
        for (auto peer : *msg->peers()) {
            if (peer->address()) {
                node.peers.emplace_back(peer->address()->c_str());
            }
        }
        return true;
    };

    int saveGraph(const std::string& file_name, const std::string& ignore_address = "") {
        std::ofstream file;
        file.open(file_name);
        if (file.fail()) {
            std::cout << strerror(errno) << " - Error saving graph\r\n";
            return errno;
        }
        file << "digraph {\r\n";
        for (const auto& node : _nodes) {
            const auto& coords = node.second.info.coordinates;
            file << "  \"" << node.first << "\" [label=\"" << node.second.info.name << "\" pos=\""
                 << coords->x() << "," << coords->y() << "\"]\r\n";
            for (const auto& peer : node.second.peers) {
                if (peer != ignore_address && peer != node.first) {
                    file << "  \"" << node.first << "\" -> \"" << peer << "\"\r\n";
                }
            }
        }
        file << "}\r\n";
        file.close();
        return 0;
    }

private:
    std::unordered_map<std::string, Node> _nodes;
};

}  // namespace vsm
