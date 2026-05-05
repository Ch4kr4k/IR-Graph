#pragma once
#include <QGraphicsView>

/// QGraphicsView subclass with:
///   - Middle-click drag to pan
///   - Scroll-wheel zoom (centred on cursor)
class GraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphView(QWidget* parent = nullptr);

    void resetZoom();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool   panning_{false};
    QPoint lastPanPos_;

    static constexpr double kZoomFactor = 1.15;
    static constexpr double kMinScale   = 0.02;
    static constexpr double kMaxScale   = 10.0;
};
