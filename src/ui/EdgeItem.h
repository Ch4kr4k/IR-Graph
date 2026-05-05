#pragma once
#include "EdgeLabelItem.h"
#include "graph/core/Graph.h"
#include "graph/layout/GraphvizLayout.h"
#include <QGraphicsObject>
#include <QPainterPath>
#include <QPointF>
#include <vector>

class EdgeItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit EdgeItem(EdgeId edgeId,
                      const std::vector<QPointF>& splinePoints,
                      const QPointF& labelPos,
                      const QString& labelText,
                      QGraphicsItem* parent = nullptr);

    EdgeId edgeId() const { return edgeId_; }

    enum class State { Normal, Highlighted, Dimmed };
    void setState(State s);

    // QGraphicsItem interface
    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;
    void         paint(QPainter* painter,
                       const QStyleOptionGraphicsItem* option,
                       QWidget* widget) override;

private:
    void buildPath();
    static void drawArrowhead(QPainter* painter, const QPointF& tip, const QPointF& before);

    EdgeId          edgeId_;
    QPainterPath    path_;
    QRectF          bbox_;
    EdgeLabelItem*  labelItem_{nullptr};
    State           state_{State::Normal};

    static constexpr double kArrowLen   = 9.0;
    static constexpr double kArrowWidth = 4.5;
};
