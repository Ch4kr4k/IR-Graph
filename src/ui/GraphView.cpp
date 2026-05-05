#include "GraphView.h"

#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

GraphView::GraphView(QWidget* parent) : QGraphicsView(parent) {
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing |
                         QGraphicsView::DontSavePainterState);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setBackgroundBrush(QColor(0x1a, 0x1a, 0x1a));
    setFrameStyle(QFrame::NoFrame);
}

void GraphView::resetZoom() {
    resetTransform();
}

void GraphView::wheelEvent(QWheelEvent* event) {
    const double angle = event->angleDelta().y();
    if (angle == 0.0) { QGraphicsView::wheelEvent(event); return; }

    // Clamp zoom range
    const double currentScale = transform().m11();
    const double factor = (angle > 0) ? kZoomFactor : (1.0 / kZoomFactor);
    const double newScale = currentScale * factor;
    if (newScale < kMinScale || newScale > kMaxScale) return;

    scale(factor, factor);
    event->accept();
}

void GraphView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_    = true;
        lastPanPos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void GraphView::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        const QPoint delta = event->pos() - lastPanPos_;
        lastPanPos_ = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void GraphView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && panning_) {
        panning_ = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
