#pragma once
#include "StringPool.h"
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

using NodeId = uint32_t;
using EdgeId = uint32_t;
constexpr NodeId INVALID_NODE = UINT32_MAX;
constexpr EdgeId INVALID_EDGE = UINT32_MAX;

/// Per-node payload stored in flat arrays.
struct NodeData {
    StringPool::StringId nameId{StringPool::INVALID}; ///< filename only (no path)
    StringPool::StringId pathId{StringPool::INVALID}; ///< full absolute path
    uint32_t             asilLevel{0};                ///< 0=none 1=A 2=B 3=C 4=D
    std::vector<EdgeId>  outEdges;
    std::vector<EdgeId>  inEdges;
};

/// Per-edge payload stored in flat arrays.
struct EdgeData {
    NodeId               from{INVALID_NODE};
    NodeId               to{INVALID_NODE};
    StringPool::StringId labelId{StringPool::INVALID}; ///< name of the included file
    uint32_t             lineNumber{0};
    bool                 isAngled{false};              ///< <angled> vs "quoted"
};

/// Directed dependency graph stored in flat arrays (32-bit indices).
class Graph {
public:
    /// Add a node for the given full path.  Returns the existing id if already present.
    NodeId addNode(std::string_view fullPath, uint32_t asilLevel = 0);

    /// Add a directed edge from→to.  Deduplicates (returns existing EdgeId).
    EdgeId addEdge(NodeId from, NodeId to, uint32_t lineNumber, bool isAngled);

    const NodeData& node(NodeId id) const { return nodes_[id]; }
    const EdgeData& edge(EdgeId id) const { return edges_[id]; }
    NodeData&       node(NodeId id)       { return nodes_[id]; }

    std::size_t nodeCount() const { return nodes_.size(); }
    std::size_t edgeCount() const { return edges_.size(); }

    const StringPool& strings() const { return pool_; }

    /// Returns INVALID_NODE if not found.
    NodeId findNode(std::string_view fullPath) const;

    void clear();

private:
    std::vector<NodeData>                      nodes_;
    std::vector<EdgeData>                      edges_;
    StringPool                                 pool_;
    std::unordered_map<std::string, NodeId>    pathToNode_;
};
