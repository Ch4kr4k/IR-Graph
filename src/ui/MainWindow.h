#pragma once
#include "GraphScene.h"
#include "GraphView.h"
#include "graph/core/Graph.h"
#include "graph/layout/GraphvizLayout.h"
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <memory>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onOpenDirectory();
    void onOpenFiles();
    void onSearchChanged(const QString& text);
    void onNodeSelected(NodeId nodeId);

private:
    void buildMenus();
    void buildToolbar();
    void buildSidePanel();
    void loadGraph();
    void applyDarkScrollBars();

    // ── Data ─────────────────────────────────────────────────────────────
    std::unique_ptr<Graph>       graph_;
    GraphLayout                  layout_;

    // ── Widgets ──────────────────────────────────────────────────────────
    GraphView*   view_        {nullptr};
    GraphScene*  scene_       {nullptr};
    QLineEdit*   searchBox_   {nullptr};
    QDockWidget* detailDock_  {nullptr};
    QLabel*      detailPath_  {nullptr};
    QLabel*      detailAsil_  {nullptr};
    QListWidget* incomingList_{nullptr};
    QListWidget* outgoingList_{nullptr};
};
