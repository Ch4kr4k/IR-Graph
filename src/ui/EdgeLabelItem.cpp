#include "EdgeLabelItem.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

// Colours ─────────────────────────────────────────────────────────────────────
static const QColor kLabelBg    {0x1a, 0x1a, 0x1a, 200}; // semi-transparent
static const QColor kLabelBorder{0x60, 0x60, 0x60};
static const QColor kLabelText  {0xe0, 0xe0, 0xe0};
static const QColor kLabelBorderHi{0x4a, 0x90, 0xd9};
static const QColor kLabelTextDim {0x44, 0x44, 0x44};

static QFont labelFont() {
    QFont f("monospace", 8);
    f.setStyleHint(QFont::Monospace);
    return f;
}

EdgeLabelItem::EdgeLabelItem(const QString& text, QGraphicsItem* parent)
    : QGraphicsObject(parent), text_(text)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setZValue(2.0);
}

void EdgeLabelItem::setText(const QString& text) {
    prepareGeometryChange();
    text_ = text;
    update();
}

void EdgeLabelItem::setState(State s) {
    if (state_ == s) return;
    state_ = s;
    update();
}

QRectF EdgeLabelItem::boundingRect() const {
    QFontMetrics fm(labelFont());
    const int tw = fm.horizontalAdvance(text_);
    const int th = fm.height();
    return QRectF(-tw / 2.0 - kPad, -th / 2.0 - kVPad,
                  tw + kPad * 2,    th + kVPad * 2);
}

void EdgeLabelItem::paint(QPainter* painter,
                          const QStyleOptionGraphicsItem* option,
                          QWidget* /*widget*/)
{
    const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
        painter->worldTransform());
    if (lod < 0.35) return; // hidden at low zoom

    const QRectF r = boundingRect();
    const QColor border = hovered_ ? kLabelBorderHi : kLabelBorder;
    const QColor text   = (state_ == State::Dimmed) ? kLabelTextDim : kLabelText;

    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setBrush(kLabelBg);
    painter->setPen(QPen(border, 0.8));
    painter->drawRect(r);

    painter->setFont(labelFont());
    painter->setPen(text);
    painter->drawText(r, Qt::AlignCenter | Qt::TextSingleLine, text_);
}

void EdgeLabelItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    hovered_ = true;
    update();
    emit labelHovered(true);
    QGraphicsObject::hoverEnterEvent(event);
}

void EdgeLabelItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    hovered_ = false;
    update();
    emit labelHovered(false);
    QGraphicsObject::hoverLeaveEvent(event);
}
