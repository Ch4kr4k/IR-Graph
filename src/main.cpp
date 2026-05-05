#include "ui/MainWindow.h"

#include <spdlog/spdlog.h>

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("IR_Graph starting");

    QApplication app(argc, argv);
    app.setApplicationName("IR_Graph");
    app.setApplicationDisplayName("IR_Graph");
    app.setApplicationVersion("1.0.0");

    // ── Dark Fusion palette ───────────────────────────────────────────────
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette p;
    p.setColor(QPalette::Window,          QColor(0x1a, 0x1a, 0x1a));
    p.setColor(QPalette::WindowText,      QColor(0xe0, 0xe0, 0xe0));
    p.setColor(QPalette::Base,            QColor(0x2d, 0x2d, 0x2d));
    p.setColor(QPalette::AlternateBase,   QColor(0x22, 0x22, 0x22));
    p.setColor(QPalette::ToolTipBase,     QColor(0x2d, 0x2d, 0x2d));
    p.setColor(QPalette::ToolTipText,     QColor(0xe0, 0xe0, 0xe0));
    p.setColor(QPalette::Text,            QColor(0xe0, 0xe0, 0xe0));
    p.setColor(QPalette::Button,          QColor(0x2d, 0x2d, 0x2d));
    p.setColor(QPalette::ButtonText,      QColor(0xe0, 0xe0, 0xe0));
    p.setColor(QPalette::BrightText,      Qt::red);
    p.setColor(QPalette::Link,            QColor(0x4a, 0x90, 0xd9));
    p.setColor(QPalette::Highlight,       QColor(0x4a, 0x90, 0xd9));
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Light,           QColor(0x40, 0x40, 0x40));
    p.setColor(QPalette::Midlight,        QColor(0x35, 0x35, 0x35));
    p.setColor(QPalette::Dark,            QColor(0x14, 0x14, 0x14));
    p.setColor(QPalette::Mid,             QColor(0x20, 0x20, 0x20));
    p.setColor(QPalette::Shadow,          QColor(0x0a, 0x0a, 0x0a));
    app.setPalette(p);

    // Minimal stylesheet for menu bars and dock title bars
    app.setStyleSheet(
        "QMenuBar          { background: #1a1a1a; color: #e0e0e0; }"
        "QMenuBar::item:selected { background: #4a90d9; }"
        "QMenu             { background: #2d2d2d; color: #e0e0e0; border: 1px solid #404040; }"
        "QMenu::item:selected    { background: #4a90d9; }"
        "QToolBar          { background: #222222; border-bottom: 1px solid #333333; spacing: 4px; }"
        "QToolButton       { color: #e0e0e0; background: transparent; border: none; padding: 3px 6px; }"
        "QToolButton:hover { background: #3a3a3a; border-radius: 3px; }"
        "QStatusBar        { background: #1a1a1a; color: #909090; }"
        "QDockWidget::title { background: #222222; color: #c0c0c0; padding-left: 6px; }"
        "QScrollBar:vertical   { background: #1a1a1a; width: 10px; }"
        "QScrollBar::handle:vertical   { background: #404040; border-radius: 4px; min-height: 20px; }"
        "QScrollBar:horizontal { background: #1a1a1a; height: 10px; }"
        "QScrollBar::handle:horizontal { background: #404040; border-radius: 4px; min-width: 20px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }"
        "QProgressDialog   { background: #2d2d2d; color: #e0e0e0; }"
        "QLabel            { color: #e0e0e0; }"
    );

    MainWindow win;
    win.resize(1440, 900);
    win.show();

    spdlog::info("Window shown");
    return app.exec();
}
