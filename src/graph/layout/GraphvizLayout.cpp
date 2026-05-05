#include "GraphvizLayout.h"

#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Plain-format parser
// ---------------------------------------------------------------------------
// After gvRenderData(..., "plain", ...) the buffer looks like:
//   graph <scale> <w> <h>
//   node <name> <x> <y> <w> <h> <label> ...
//   edge <tail> <head> <n> <x1> <y1> … <xn> <yn> [<label> <lx> <ly>] ...
//   stop
// Coordinates are in inches; multiply by kDPI to get scene pixels.
// ---------------------------------------------------------------------------

namespace {

struct PlainNode {
    double x{}, y{};
};
struct PlainEdge {
    std::string             tail, head;
    std::vector<QPointF>    pts;
};

struct PlainGraph {
    double                                    scale{1.0};
    double                                    width{}, height{};
    std::unordered_map<std::string, PlainNode> nodes;
    std::vector<PlainEdge>                    edges;
};

static PlainGraph parsePlain(const char* buf, unsigned len) {
    PlainGraph pg;
    std::string text(buf, len);
    std::istringstream in(text);
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;

        if (tag == "graph") {
            ls >> pg.scale >> pg.width >> pg.height;
        } else if (tag == "node") {
            PlainNode pn;
            std::string name;
            ls >> name >> pn.x >> pn.y;
            pn.x *= pg.scale;
            pn.y *= pg.scale;
            pg.nodes[name] = pn;
        } else if (tag == "edge") {
            PlainEdge pe;
            int n = 0;
            ls >> pe.tail >> pe.head >> n;
            pe.pts.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) {
                double x{}, y{};
                ls >> x >> y;
                pe.pts.emplace_back(x * pg.scale, y * pg.scale);
            }
            pg.edges.push_back(std::move(pe));
        } else if (tag == "stop") {
            break;
        }
    }
    return pg;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
GraphLayout GraphvizLayout::layout(const Graph& graph) {
    GraphLayout result;
    if (graph.nodeCount() == 0) {
        result.valid = true;
        return result;
    }

    GVC_t*    gvc = gvContext();
    Agraph_t* g   = agopen(const_cast<char*>("G"), Agdirected, nullptr);

    // ── Global graph attributes (dot hierarchical layout) ───────────────────
    // rankdir=LR gives a left-to-right include hierarchy (root files on left,
    // deep dependencies on right), which mirrors how #include trees read.
    agattr(g, AGRAPH, const_cast<char*>("rankdir"),     const_cast<char*>("LR"));
    agattr(g, AGRAPH, const_cast<char*>("ranksep"),     const_cast<char*>("1.2"));
    agattr(g, AGRAPH, const_cast<char*>("nodesep"),     const_cast<char*>("0.4"));
    agattr(g, AGRAPH, const_cast<char*>("splines"),     const_cast<char*>("ortho"));
    agattr(g, AGRAPH, const_cast<char*>("outputorder"), const_cast<char*>("edgesfirst"));
    agattr(g, AGRAPH, const_cast<char*>("concentrate"), const_cast<char*>("true"));

    // ── Default node attributes ──────────────────────────────────────────────
    agattr(g, AGNODE, const_cast<char*>("shape"),     const_cast<char*>("box"));
    agattr(g, AGNODE, const_cast<char*>("width"),     const_cast<char*>("1.6"));
    agattr(g, AGNODE, const_cast<char*>("height"),    const_cast<char*>("0.45"));
    agattr(g, AGNODE, const_cast<char*>("fixedsize"), const_cast<char*>("true"));

    // ── Create nodes ─────────────────────────────────────────────────────────
    std::vector<Agnode_t*> gvNodes(graph.nodeCount(), nullptr);
    for (NodeId i = 0; i < static_cast<NodeId>(graph.nodeCount()); ++i) {
        std::string name = "n" + std::to_string(i);
        gvNodes[i] = agnode(g, const_cast<char*>(name.c_str()), 1);
    }

    // ── Create edges ─────────────────────────────────────────────────────────
    // Build a map (from,to) → EdgeId for later matching.
    // (Duplicate (from,to) pairs have been deduplicated in Graph::addEdge.)
    std::unordered_map<uint64_t, EdgeId> edgeKey; // key = from<<32 | to

    for (EdgeId i = 0; i < static_cast<EdgeId>(graph.edgeCount()); ++i) {
        const EdgeData& ed = graph.edge(i);
        if (!gvNodes[ed.from] || !gvNodes[ed.to]) continue;
        std::string name = "e" + std::to_string(i);
        agedge(g, gvNodes[ed.from], gvNodes[ed.to],
               const_cast<char*>(name.c_str()), 1);
        uint64_t key = (static_cast<uint64_t>(ed.from) << 32) | ed.to;
        edgeKey[key] = i;
    }

    // ── Run sfdp ─────────────────────────────────────────────────────────────
    spdlog::info("GraphvizLayout: running dot on {} nodes, {} edges",
                 graph.nodeCount(), graph.edgeCount());

    int rc = gvLayout(gvc, g, "dot");
    if (rc != 0) {
        spdlog::error("gvLayout(sfdp) failed with code {}", rc);
        gvFreeContext(gvc);
        agclose(g);
        return result;
    }

    // ── Render to plain text ──────────────────────────────────────────────────
    char*        plainBuf = nullptr;
    unsigned int plainLen = 0;
    rc = gvRenderData(gvc, g, "plain", &plainBuf, &plainLen);
    if (rc != 0 || !plainBuf) {
        spdlog::error("gvRenderData(plain) failed");
        gvFreeLayout(gvc, g);
        gvFreeContext(gvc);
        agclose(g);
        return result;
    }

    PlainGraph pg = parsePlain(plainBuf, plainLen);
    gvFreeRenderData(plainBuf);

    gvFreeLayout(gvc, g);
    gvFreeContext(gvc);
    agclose(g);

    // ── Convert to GraphLayout ────────────────────────────────────────────────
    // In the plain format, y=0 is at the bottom; Qt y=0 is at the top.
    // graphHeight is in inches; multiply by kDPI for scene pixels.
    const double W = pg.width  * kDPI;
    const double H = pg.height * kDPI;

    result.boundingBox = QRectF(0.0, 0.0, W, H);
    result.nodes.resize(graph.nodeCount());
    result.edges.resize(graph.edgeCount());

    // Node positions
    for (NodeId i = 0; i < static_cast<NodeId>(graph.nodeCount()); ++i) {
        std::string name = "n" + std::to_string(i);
        auto it = pg.nodes.find(name);
        result.nodes[i].nodeId = i;
        if (it != pg.nodes.end()) {
            result.nodes[i].pos  = QPointF(it->second.x * kDPI,
                                           H - it->second.y * kDPI); // flip Y
            result.nodes[i].size = QSizeF(1.6 * kDPI, 0.45 * kDPI);
        }
    }

    // Edge splines
    for (const PlainEdge& pe : pg.edges) {
        // Recover the NodeId indices from the name "nXXX"
        NodeId from = INVALID_NODE, to = INVALID_NODE;
        if (pe.tail.size() > 1 && pe.tail[0] == 'n') {
            from = static_cast<NodeId>(std::stoul(pe.tail.substr(1)));
        }
        if (pe.head.size() > 1 && pe.head[0] == 'n') {
            to = static_cast<NodeId>(std::stoul(pe.head.substr(1)));
        }
        if (from == INVALID_NODE || to == INVALID_NODE) continue;

        uint64_t key = (static_cast<uint64_t>(from) << 32) | to;
        auto eit = edgeKey.find(key);
        if (eit == edgeKey.end()) continue;

        EdgeId eid = eit->second;
        auto& el   = result.edges[eid];
        el.edgeId  = eid;
        el.splinePoints.reserve(pe.pts.size());
        for (const QPointF& p : pe.pts) {
            el.splinePoints.emplace_back(p.x() * kDPI, H - p.y() * kDPI);
        }
        if (!el.splinePoints.empty()) {
            std::size_t mid = el.splinePoints.size() / 2;
            el.labelPos = el.splinePoints[mid];
        }
    }

    result.valid = true;
    spdlog::info("GraphvizLayout: done.  Bounding box {:.0f}×{:.0f} px", W, H);
    return result;
}
