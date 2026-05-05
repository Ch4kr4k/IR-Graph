#pragma once
#include "EdgeItem.h"
#include "NodeItem.h"
#include "graph/core/Graph.h"
#include "graph/layout/GraphvizLayout.h"
#include <QGraphicsScene>
#include <unordered_map>
#include <vector>

class GraphScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit GraphScene(QObject* parent = nullptr);

    /// Clear the scene and rebuild it from the given graph + layout.
    void populate(const Graph& graph, const GraphLayout& layout);

    /// Highlight @p nodeId and its immediate neighbours; dim everything else.
    /// Pass INVALID_NODE to clear selection.
    void highlightNode(NodeId nodeId);

    /// Show only nodes whose name contains @p text (case-insensitive) plus their
    /// direct neighbours.  Empty text restores all nodes.
    void filterByText(const QString& text);

    const Graph* graph() const { return graph_; }

signals:
    void nodeSelected(NodeId nodeId);

private:
    void resetAllStates();

    const Graph*                            graph_{nullptr};
    std::vector<NodeItem*>                  nodeItems_;   // indexed by NodeId
    std::vector<EdgeItem*>                  edgeItems_;   // indexed by EdgeId
    NodeId                                  selectedNode_{INVALID_NODE};
};
