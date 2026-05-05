#include "EdgeItem.h"

#include <QLineF>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QVector2D>
#include <cmath>

// ── Colours ────────────────────────────────────────────────────────────────
static constexpr QColor kEdgeNormal {0x60, 0x60, 0x60};
static constexpr QColor kEdgeHigh   {0x4a, 0x90, 0xd9};
static constexpr QColor kEdgeDim    {0x2a, 0x2a, 0x2a};

EdgeItem::EdgeItem(EdgeId edgeId,
                   const std::vector<QPointF>& splinePoints,
                   const QPointF& labelPos,
                   const QString& labelText,
                   QGraphicsItem* parent)
    : QGraphicsObject(parent), edgeId_(edgeId)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setZValue(0.0);
    setAcceptHoverEvents(false);

    // Build bezier path from spline control points
    if (!splinePoints.empty()) {
        path_.moveTo(splinePoints[0]);
        std::size_t i = 1;
        while (i + 2 < splinePoints.size()) {
            path_.cubicTo(splinePoints[i], splinePoints[i + 1], splinePoints[i + 2]);
            i += 3;
        }
        // If leftover linear segment
        while (i < splinePoints.size()) {
            path_.lineTo(splinePoints[i++]);
        }
    }

    // Fallback straight line when graphviz gave no spline points
    if (path_.isEmpty() && splinePoints.size() >= 2) {
        path_.moveTo(splinePoints.front());
        path_.lineTo(splinePoints.back());
    }

    bbox_ = path_.boundingRect().adjusted(-kArrowLen, -kArrowLen,
                                           kArrowLen,  kArrowLen);

    // ── Edge label ─────────────────────────────────────────────────────────
    if (!labelText.isEmpty()) {
        labelItem_ = new EdgeLabelItem(labelText, this);
        labelItem_->setPos(labelPos);

        // Rotate label to match edge direction at midpoint
        if (splinePoints.size() >= 2) {
            std::size_t mid = splinePoints.size() / 2;
            const QPointF& a = splinePoints[mid > 0 ? mid - 1 : 0];
            const QPointF& b = splinePoints[mid];
            qreal angle = QLineF(a, b).angle(); // degrees, 0 = right
            // Keep text readable (flip if upside-down)
            if (angle > 90.0 && angle < 270.0) angle += 180.0;
            labelItem_->setRotation(-angle);
        }
    }
}

void EdgeItem::setState(State s) {
    if (state_ == s) return;
    state_ = s;
    if (labelItem_) {
        labelItem_->setState(static_cast<EdgeLabelItem::State>(
            static_cast<int>(s)));
    }
    update();
}

QRectF EdgeItem::boundingRect() const {
    return bbox_;
}

QPainterPath EdgeItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(6.0);
    return stroker.createStroke(path_);
}

void EdgeItem::paint(QPainter* painter,
                     const QStyleOptionGraphicsItem* option,
                     QWidget* /*widget*/)
{
    const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
        painter->worldTransform());

    QColor col;
    qreal  penW;
    switch (state_) {
    case State::Highlighted: col = kEdgeHigh; penW = 1.8; break;
    case State::Dimmed:      col = kEdgeDim;  penW = 0.8; break;
    default:                 col = kEdgeNormal; penW = 1.0; break;
    }

    painter->setRenderHint(QPainter::Antialiasing, lod >= 0.15);
    painter->setPen(QPen(col, penW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path_);

    // Arrowhead (skip at very low LOD)
    if (lod >= 0.15 && path_.elementCount() >= 2) {
        const int n = path_.elementCount();
        QPointF tip    = path_.elementAt(n - 1);
        QPointF before = path_.elementAt(n - 2);
        painter->setBrush(col);
        painter->setPen(Qt::NoPen);
        drawArrowhead(painter, tip, before);
    }
}

void EdgeItem::drawArrowhead(QPainter* painter,
                              const QPointF& tip,
                              const QPointF& before)
{
    QVector2D d(tip - before);
    if (d.length() < 0.001) return;
    d.normalize();
    QVector2D perp(-d.y(), d.x());

    QPointF p1 = tip;
    QPointF p2 = tip - (d * kArrowLen + perp * kArrowWidth).toPointF();
    QPointF p3 = tip - (d * kArrowLen - perp * kArrowWidth).toPointF();
    painter->drawPolygon(QPolygonF({p1, p2, p3}));
}
