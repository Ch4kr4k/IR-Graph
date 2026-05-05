#include "Graph.h"
#include <string>

NodeId Graph::addNode(std::string_view fullPath, uint32_t asilLevel) {
    std::string key(fullPath);
    auto it = pathToNode_.find(key);
    if (it != pathToNode_.end()) return it->second;

    // Extract filename (everything after the last '/')
    auto slash = fullPath.rfind('/');
    std::string_view nameOnly = (slash != std::string_view::npos)
                                    ? fullPath.substr(slash + 1)
                                    : fullPath;

    NodeData data;
    data.nameId    = pool_.intern(nameOnly);
    data.pathId    = pool_.intern(fullPath);
    data.asilLevel = asilLevel;

    NodeId id = static_cast<NodeId>(nodes_.size());
    nodes_.push_back(std::move(data));
    pathToNode_[key] = id;
    return id;
}

EdgeId Graph::addEdge(NodeId from, NodeId to, uint32_t lineNumber, bool isAngled) {
    // Deduplicate: one edge per (from, to) pair
    for (EdgeId eid : nodes_[from].outEdges) {
        if (edges_[eid].to == to) return eid;
    }

    EdgeData data;
    data.from       = from;
    data.to         = to;
    data.labelId    = nodes_[to].nameId;  // edge label = name of the included file
    data.lineNumber = lineNumber;
    data.isAngled   = isAngled;

    EdgeId id = static_cast<EdgeId>(edges_.size());
    edges_.push_back(std::move(data));
    nodes_[from].outEdges.push_back(id);
    nodes_[to].inEdges.push_back(id);
    return id;
}

NodeId Graph::findNode(std::string_view fullPath) const {
    auto it = pathToNode_.find(std::string(fullPath));
    return (it != pathToNode_.end()) ? it->second : INVALID_NODE;
}

void Graph::clear() {
    nodes_.clear();
    edges_.clear();
    pool_       = StringPool{};
    pathToNode_.clear();
}
