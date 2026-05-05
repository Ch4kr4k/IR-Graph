#include "GraphScene.h"

#include <QGraphicsRectItem>

GraphScene::GraphScene(QObject* parent) : QGraphicsScene(parent) {
    setBackgroundBrush(QColor(0x1a, 0x1a, 0x1a));
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
}

void GraphScene::populate(const Graph& graph, const GraphLayout& layout) {
    clear();
    nodeItems_.clear();
    edgeItems_.clear();
    graph_        = &graph;
    selectedNode_ = INVALID_NODE;

    if (!layout.valid) return;

    nodeItems_.resize(graph.nodeCount(), nullptr);
    edgeItems_.resize(graph.edgeCount(), nullptr);

    // ── Edges first (drawn under nodes) ──────────────────────────────────
    for (EdgeId eid = 0; eid < static_cast<EdgeId>(graph.edgeCount()); ++eid) {
        const EdgeData& ed  = graph.edge(eid);
        const EdgeLayout& el = layout.edges[eid];

        QString labelText = QString::fromStdString(
            std::string(graph.strings().get(ed.labelId)));

        // Fallback: straight line between node centres
        std::vector<QPointF> pts = el.splinePoints;
        if (pts.empty() && eid < layout.nodes.size()) {
            const auto& fn = layout.nodes[ed.from];
            const auto& tn = layout.nodes[ed.to];
            pts = { fn.pos, tn.pos };
        }

        QPointF labelPos = el.labelPos;
        if (pts.size() >= 2 && labelPos.isNull())
            labelPos = (pts.front() + pts.back()) / 2.0;

        auto* item = new EdgeItem(eid, pts, labelPos, labelText);
        addItem(item);
        edgeItems_[eid] = item;

        // Wire label hover → show tooltip or later: signal the scene
        // (no action needed here; EdgeLabelItem handles its own hover highlight)
    }

    // ── Nodes ─────────────────────────────────────────────────────────────
    for (NodeId nid = 0; nid < static_cast<NodeId>(graph.nodeCount()); ++nid) {
        const NodeData& nd = graph.node(nid);
        const NodeLayout& nl = layout.nodes[nid];

        QString label = QString::fromStdString(
            std::string(graph.strings().get(nd.nameId)));

        auto* item = new NodeItem(nid, label);
        item->setPos(nl.pos);
        addItem(item);
        nodeItems_[nid] = item;

        connect(item, &NodeItem::nodeClicked, this, &GraphScene::highlightNode);
    }

    setSceneRect(layout.boundingBox.adjusted(-50, -50, 50, 50));
}

void GraphScene::highlightNode(NodeId nodeId) {
    if (nodeId == selectedNode_) {
        // Toggle off
        selectedNode_ = INVALID_NODE;
        resetAllStates();
        emit nodeSelected(INVALID_NODE);
        return;
    }

    selectedNode_ = nodeId;
    emit nodeSelected(nodeId);

    if (!graph_ || nodeId >= static_cast<NodeId>(graph_->nodeCount())) return;

    // Collect nodes and edges involved
    std::vector<bool> nodeVisible(graph_->nodeCount(), false);
    std::vector<bool> edgeVisible(graph_->edgeCount(), false);

    nodeVisible[nodeId] = true;

    const NodeData& nd = graph_->node(nodeId);
    for (EdgeId eid : nd.outEdges) {
        edgeVisible[eid] = true;
        nodeVisible[graph_->edge(eid).to] = true;
    }
    for (EdgeId eid : nd.inEdges) {
        edgeVisible[eid] = true;
        nodeVisible[graph_->edge(eid).from] = true;
    }

    for (NodeId i = 0; i < static_cast<NodeId>(nodeItems_.size()); ++i) {
        if (!nodeItems_[i]) continue;
        nodeItems_[i]->setState(nodeVisible[i]
            ? (i == nodeId ? NodeItem::State::Highlighted : NodeItem::State::Normal)
            : NodeItem::State::Dimmed);
    }
    for (EdgeId i = 0; i < static_cast<EdgeId>(edgeItems_.size()); ++i) {
        if (!edgeItems_[i]) continue;
        edgeItems_[i]->setState(edgeVisible[i]
            ? EdgeItem::State::Highlighted
            : EdgeItem::State::Dimmed);
    }
}

void GraphScene::filterByText(const QString& text) {
    if (!graph_) return;

    if (text.isEmpty()) {
        for (auto* n : nodeItems_) if (n) { n->setVisible(true);  n->setState(NodeItem::State::Normal); }
        for (auto* e : edgeItems_) if (e) { e->setVisible(true);  e->setState(EdgeItem::State::Normal); }
        return;
    }

    const QString lower = text.toLower();

    // Determine which nodes match
    std::vector<bool> matched(graph_->nodeCount(), false);
    for (NodeId i = 0; i < static_cast<NodeId>(graph_->nodeCount()); ++i) {
        std::string name = std::string(graph_->strings().get(graph_->node(i).nameId));
        if (QString::fromStdString(name).toLower().contains(lower))
            matched[i] = true;
    }

    // Expand to direct neighbours
    std::vector<bool> visible(graph_->nodeCount(), false);
    for (NodeId i = 0; i < static_cast<NodeId>(graph_->nodeCount()); ++i) {
        if (!matched[i]) continue;
        visible[i] = true;
        for (EdgeId eid : graph_->node(i).outEdges) visible[graph_->edge(eid).to]   = true;
        for (EdgeId eid : graph_->node(i).inEdges)  visible[graph_->edge(eid).from] = true;
    }

    for (NodeId i = 0; i < static_cast<NodeId>(nodeItems_.size()); ++i) {
        if (nodeItems_[i]) nodeItems_[i]->setVisible(visible[i]);
    }
    for (EdgeId i = 0; i < static_cast<EdgeId>(edgeItems_.size()); ++i) {
        if (!edgeItems_[i]) continue;
        const EdgeData& ed = graph_->edge(i);
        edgeItems_[i]->setVisible(visible[ed.from] && visible[ed.to]);
    }
}

void GraphScene::resetAllStates() {
    for (auto* n : nodeItems_) if (n) n->setState(NodeItem::State::Normal);
    for (auto* e : edgeItems_) if (e) e->setState(EdgeItem::State::Normal);
}
