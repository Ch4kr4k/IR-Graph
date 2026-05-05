#pragma once
#include "graph/core/Graph.h"
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <vector>

/// Position / geometry for a single node returned from sfdp layout.
struct NodeLayout {
    NodeId  nodeId{INVALID_NODE};
    QPointF pos;      ///< centre of the node in scene units (pixels at 72 DPI)
    QSizeF  size;     ///< width × height in scene units
};

/// Bezier spline + metadata for a single directed edge.
struct EdgeLayout {
    EdgeId              edgeId{INVALID_EDGE};
    std::vector<QPointF> splinePoints; ///< flat array: moveTo p0, cubicTo p1 p2 p3, …
    QPointF             labelPos;      ///< midpoint of the spline (label placement)
};

/// Full layout result returned by GraphvizLayout::layout().
struct GraphLayout {
    std::vector<NodeLayout> nodes;
    std::vector<EdgeLayout> edges;
    QRectF                  boundingBox;
    bool                    valid{false};
};

/// Wrapper around Graphviz's sfdp engine.
class GraphvizLayout {
public:
    /// Lay out the graph synchronously.  May take seconds for large graphs.
    static GraphLayout layout(const Graph& graph);

private:
    static constexpr double kDPI = 72.0; ///< graphviz internal unit = 1/72 inch
};
