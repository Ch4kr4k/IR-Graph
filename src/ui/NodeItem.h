#pragma once
#include "graph/core/Graph.h"
#include <QGraphicsObject>
#include <QRectF>

class NodeItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit NodeItem(NodeId nodeId, const QString& label, QGraphicsItem* parent = nullptr);

    NodeId nodeId() const { return nodeId_; }

    // Highlight states
    enum class State { Normal, Highlighted, Dimmed };
    void setState(State s);
    State state() const { return state_; }

    // QGraphicsItem interface
    QRectF       boundingRect() const override;
    void         paint(QPainter* painter,
                       const QStyleOptionGraphicsItem* option,
                       QWidget* widget) override;

signals:
    void nodeClicked(NodeId id);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    NodeId  nodeId_;
    QString label_;
    State   state_  {State::Normal};
    bool    hovered_{false};

    // Node visual size (scene pixels)
    static constexpr double kW = 115.0;
    static constexpr double kH = 32.0;
    static constexpr double kR = 5.0;  // corner radius
};
