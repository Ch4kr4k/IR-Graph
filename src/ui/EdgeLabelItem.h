#pragma once
#include <QGraphicsObject>
#include <QString>

/// A small rectangular label box drawn at the midpoint of an edge.
/// It is a child of EdgeItem so it moves with the edge automatically.
class EdgeLabelItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit EdgeLabelItem(const QString& text, QGraphicsItem* parent = nullptr);

    void setText(const QString& text);

    // Highlight / dim alongside the parent edge
    enum class State { Normal, Highlighted, Dimmed };
    void setState(State s);

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void   paint(QPainter* painter,
                 const QStyleOptionGraphicsItem* option,
                 QWidget* widget) override;

signals:
    void labelHovered(bool entered);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    QString text_;
    State   state_{State::Normal};
    bool    hovered_{false};

    static constexpr int kPad = 6;   // horizontal padding (pixels)
    static constexpr int kVPad = 3;  // vertical padding
};
