#include "NodeItem.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>

// ── Colour palette ────────────────────────────────────────────────────────────
static constexpr QColor kNormalBg    {0x2d, 0x2d, 0x2d};
static constexpr QColor kNormalBorder{0x50, 0x50, 0x50};
static constexpr QColor kHighBg     {0x4a, 0x90, 0xd9};
static constexpr QColor kHighBorder {0x6a, 0xb0, 0xff};
static constexpr QColor kDimBg      {0x1a, 0x1a, 0x1a};
static constexpr QColor kDimBorder  {0x33, 0x33, 0x33};
static constexpr QColor kNormalText {0xe0, 0xe0, 0xe0};
static constexpr QColor kHighText   {0xff, 0xff, 0xff};
static constexpr QColor kDimText    {0x55, 0x55, 0x55};
static constexpr QColor kHoverBorder{0x80, 0xb8, 0xff};

NodeItem::NodeItem(NodeId nodeId, const QString& label, QGraphicsItem* parent)
    : QGraphicsObject(parent), nodeId_(nodeId), label_(label)
{
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setAcceptHoverEvents(true);
    setZValue(1.0);
}

QRectF NodeItem::boundingRect() const {
    return QRectF(-kW / 2.0, -kH / 2.0, kW, kH);
}

void NodeItem::setState(State s) {
    if (state_ == s) return;
    state_ = s;
    update();
}

void NodeItem::paint(QPainter* painter,
                     const QStyleOptionGraphicsItem* option,
                     QWidget* /*widget*/) {
    const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
        painter->worldTransform());

    // ── LOD: extremely zoomed out → draw a dot ─────────────────────────────
    if (lod < 0.15) {
        QColor dotCol = (state_ == State::Highlighted) ? kHighBg : kNormalBg;
        painter->setPen(Qt::NoPen);
        painter->setBrush(dotCol);
        painter->drawEllipse(QRectF(-3, -3, 6, 6));
        return;
    }

    // ── Colours based on state ─────────────────────────────────────────────
    QColor bg, border, textCol;
    switch (state_) {
    case State::Highlighted:
        bg = kHighBg; border = hovered_ ? kHoverBorder : kHighBorder; textCol = kHighText;
        break;
    case State::Dimmed:
        bg = kDimBg; border = kDimBorder; textCol = kDimText;
        break;
    default:
        bg = kNormalBg; border = hovered_ ? kHoverBorder : kNormalBorder; textCol = kNormalText;
        break;
    }

    const QRectF r = boundingRect();

    // ── Background ──────────────────────────────────────────────────────────
    painter->setPen(QPen(border, 1.0));
    painter->setBrush(bg);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->drawRoundedRect(r, kR, kR);

    // ── Label (only when sufficiently zoomed in) ───────────────────────────
    if (lod >= 0.35) {
        QFont font("monospace", 8);
        font.setStyleHint(QFont::Monospace);
        painter->setFont(font);
        painter->setPen(textCol);
        painter->drawText(r.adjusted(4, 0, -4, 0),
                          Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                          label_);
    }
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        emit nodeClicked(nodeId_);
    QGraphicsObject::mousePressEvent(event);
}

void NodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    hovered_ = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void NodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    hovered_ = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}
