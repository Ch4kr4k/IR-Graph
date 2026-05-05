#include "MainWindow.h"

#include "graph/layout/GraphvizLayout.h"
#include "parser/ClangParser.h"

#include <spdlog/spdlog.h>

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QProgressDialog>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

static const QString kAsilNames[] = { "None", "ASIL-A", "ASIL-B", "ASIL-C", "ASIL-D" };

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("IR_Graph — C/C++ Include Dependency Visualizer");

    // ── Central widget ────────────────────────────────────────────────────
    graph_ = std::make_unique<Graph>();
    scene_ = new GraphScene(this);
    view_  = new GraphView(this);
    view_->setScene(scene_);
    setCentralWidget(view_);

    connect(scene_, &GraphScene::nodeSelected, this, &MainWindow::onNodeSelected);

    buildMenus();
    buildToolbar();
    buildSidePanel();

    statusBar()->showMessage("Ready — open a directory or files to parse.");
}

void MainWindow::buildMenus() {
    QMenu* fileMenu = menuBar()->addMenu("&File");

    auto* actDir = new QAction("Open &Directory…", this);
    actDir->setShortcut(QKeySequence::Open);
    connect(actDir, &QAction::triggered, this, &MainWindow::onOpenDirectory);
    fileMenu->addAction(actDir);

    auto* actFiles = new QAction("Open &Files…", this);
    actFiles->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(actFiles, &QAction::triggered, this, &MainWindow::onOpenFiles);
    fileMenu->addAction(actFiles);

    fileMenu->addSeparator();

    auto* actQuit = new QAction("&Quit", this);
    actQuit->setShortcut(QKeySequence::Quit);
    connect(actQuit, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(actQuit);

    QMenu* viewMenu = menuBar()->addMenu("&View");
    auto* actReset = new QAction("Reset &Zoom", this);
    actReset->setShortcut(QKeySequence("Ctrl+0"));
    connect(actReset, &QAction::triggered, view_, &GraphView::resetZoom);
    viewMenu->addAction(actReset);

    auto* actFit = new QAction("&Fit All", this);
    actFit->setShortcut(QKeySequence("Ctrl+F"));
    connect(actFit, &QAction::triggered, this, [this]() {
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    });
    viewMenu->addAction(actFit);
}

void MainWindow::buildToolbar() {
    QToolBar* tb = addToolBar("Main");
    tb->setMovable(false);

    auto* actDir = new QAction("Open Dir", this);
    connect(actDir, &QAction::triggered, this, &MainWindow::onOpenDirectory);
    tb->addAction(actDir);

    auto* actFiles = new QAction("Open Files", this);
    connect(actFiles, &QAction::triggered, this, &MainWindow::onOpenFiles);
    tb->addAction(actFiles);

    tb->addSeparator();

    QLabel* lbl = new QLabel("  Search: ");
    lbl->setStyleSheet("color: #e0e0e0;");
    tb->addWidget(lbl);

    searchBox_ = new QLineEdit;
    searchBox_->setPlaceholderText("filter filenames…");
    searchBox_->setMinimumWidth(220);
    searchBox_->setClearButtonEnabled(true);
    searchBox_->setStyleSheet(
        "QLineEdit { background: #2d2d2d; color: #e0e0e0; border: 1px solid #505050;"
        " border-radius: 3px; padding: 2px 4px; }");
    connect(searchBox_, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);
    tb->addWidget(searchBox_);
}

void MainWindow::buildSidePanel() {
    auto* panel = new QWidget;
    auto* vlay  = new QVBoxLayout(panel);
    vlay->setContentsMargins(8, 8, 8, 8);
    vlay->setSpacing(6);

    auto* title = new QLabel("<b>File Details</b>");
    title->setStyleSheet("color: #e0e0e0; font-size: 13px;");
    vlay->addWidget(title);

    auto* form = new QFormLayout;
    form->setSpacing(4);

    detailPath_ = new QLabel("—");
    detailPath_->setWordWrap(true);
    detailPath_->setStyleSheet("color: #b0b0b0; font-size: 11px;");
    form->addRow(new QLabel("Path:"), detailPath_);

    detailAsil_ = new QLabel("—");
    detailAsil_->setStyleSheet("color: #b0b0b0;");
    form->addRow(new QLabel("ASIL:"), detailAsil_);

    vlay->addLayout(form);

    auto mkGroup = [&](const QString& title, QListWidget*& listOut) {
        auto* grp  = new QGroupBox(title);
        grp->setStyleSheet(
            "QGroupBox { color:#a0a0a0; border:1px solid #404040; margin-top:6px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 6px; }");
        auto* gl = new QVBoxLayout(grp);
        gl->setContentsMargins(4, 4, 4, 4);
        listOut = new QListWidget;
        listOut->setStyleSheet(
            "QListWidget { background:#1a1a1a; color:#c0c0c0; border:none; font-size:11px; }");
        listOut->setMaximumHeight(180);
        gl->addWidget(listOut);
        vlay->addWidget(grp);
    };

    mkGroup("Incoming Includes", incomingList_);
    mkGroup("Outgoing Includes", outgoingList_);

    vlay->addStretch();

    panel->setMinimumWidth(250);
    panel->setMaximumWidth(340);
    panel->setStyleSheet("background: #1e1e1e;");

    detailDock_ = new QDockWidget("Details", this);
    detailDock_->setWidget(panel);
    detailDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, detailDock_);
}

// ── Slots ────────────────────────────────────────────────────────────────────

void MainWindow::onOpenDirectory() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Select source directory", QDir::homePath());
    if (dir.isEmpty()) return;

    graph_->clear();
    ClangParser parser;
    {
        QProgressDialog progress("Parsing C/C++ files…", "Cancel", 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();
        QApplication::processEvents();
        parser.parseDirectory(std::filesystem::path(dir.toStdString()), *graph_);
    }
    loadGraph();
    statusBar()->showMessage(
        QString("Loaded %1 nodes, %2 edges from %3")
            .arg(graph_->nodeCount()).arg(graph_->edgeCount()).arg(dir));
}

void MainWindow::onOpenFiles() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, "Select C/C++ source files", QDir::homePath(),
        "C/C++ files (*.c *.cpp *.cxx *.cc *.h *.hpp *.hxx);;All files (*)");
    if (files.isEmpty()) return;

    graph_->clear();
    ClangParser parser;
    std::vector<std::string> paths;
    paths.reserve(static_cast<std::size_t>(files.size()));
    for (const QString& f : files) paths.push_back(f.toStdString());
    parser.parseFiles(paths, *graph_);
    loadGraph();
    statusBar()->showMessage(
        QString("Loaded %1 nodes, %2 edges.")
            .arg(graph_->nodeCount()).arg(graph_->edgeCount()));
}

void MainWindow::loadGraph() {
    searchBox_->clear();
    onNodeSelected(INVALID_NODE);

    QProgressDialog progress("Computing sfdp layout…", nullptr, 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    QApplication::processEvents();

    layout_ = GraphvizLayout::layout(*graph_);
    scene_->populate(*graph_, layout_);
    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}

void MainWindow::onSearchChanged(const QString& text) {
    scene_->filterByText(text);
}

void MainWindow::onNodeSelected(NodeId nodeId) {
    if (!graph_ || nodeId == INVALID_NODE
        || nodeId >= static_cast<NodeId>(graph_->nodeCount())) {
        detailPath_->setText("—");
        detailAsil_->setText("—");
        incomingList_->clear();
        outgoingList_->clear();
        return;
    }

    const NodeData& nd = graph_->node(nodeId);
    detailPath_->setText(
        QString::fromStdString(std::string(graph_->strings().get(nd.pathId))));
    const uint32_t asil = nd.asilLevel;
    detailAsil_->setText(asil < 5 ? kAsilNames[asil] : "Unknown");

    incomingList_->clear();
    for (EdgeId eid : nd.inEdges) {
        NodeId src = graph_->edge(eid).from;
        incomingList_->addItem(QString::fromStdString(
            std::string(graph_->strings().get(graph_->node(src).nameId))));
    }

    outgoingList_->clear();
    for (EdgeId eid : nd.outEdges) {
        NodeId dst = graph_->edge(eid).to;
        outgoingList_->addItem(QString::fromStdString(
            std::string(graph_->strings().get(graph_->node(dst).nameId))));
    }
}
