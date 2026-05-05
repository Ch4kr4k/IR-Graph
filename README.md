# IR_Graph — C/C++ `#include` Dependency Graph Visualizer

A Linux desktop application that visualizes the `#include` dependency graph of a
C/C++ codebase at ECU-firmware scale (100 000+ files). Built with Qt 6, libclang,
and Graphviz sfdp.

---

## Features

* Real AST-based parsing via **libclang** (no regex).
* **sfdp** automatic layout for massive graphs.
* LOD rendering — nodes collapse to dots when zoomed out.
* Edge label boxes at the midpoint of every edge (shows included filename).
* Click-to-highlight: selects a node and all its connected edges/neighbours.
* Search/filter box: narrows the view to matching files + their direct neighbours.
* Side panel: full path, incoming / outgoing includes, ASIL level.
* Dark theme throughout.

---

## System Requirements

| Dependency | Min version | Ubuntu package |
|---|---|---|
| CMake | 3.20 | `cmake` |
| GCC / Clang | C++20 | `build-essential` |
| Qt 6 | 6.2 | `qt6-base-dev` |
| libclang | 14+ | `libclang-dev` |
| Graphviz dev | 2.42 | `libgraphviz-dev` |
| spdlog | 1.9 | via vcpkg or `libspdlog-dev` |
| vcpkg | any | see below |

---

## Build Instructions (Ubuntu 22.04 / 24.04)

### 1. Install system dependencies

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build \
    qt6-base-dev \
    libclang-dev \
    libgraphviz-dev \
    pkg-config \
    curl git zip unzip tar   # needed by vcpkg
```

### 2. Set up vcpkg (for spdlog)

```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT=~/vcpkg
```

### 3. Configure and build

```bash
cd IR_Graph
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)
```

A `compile_commands.json` is generated in `build/` for clangd.

### 4. Debug build

```bash
cmake -B build-debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build-debug
```

### 5. Run

```bash
./build/IR_Graph
```

---

## Usage

1. **Open Directory** — File → Open Directory (or toolbar button). IR_Graph
   recursively finds all `.c`, `.cpp`, `.h`, `.hpp` files and parses them with
   libclang.
2. **Open Files** — File → Open Files to select individual source files.
3. **Navigate** — scroll wheel to zoom, middle-click drag to pan, click a node to
   highlight it and its neighbours.
4. **Search** — type in the search box (toolbar) to filter the graph to matching
   files and their direct neighbours. Clear the box to restore the full graph.
5. **Details** — the right panel updates whenever a node is selected.

---

## Project Structure

```
src/
├── main.cpp
├── graph/
│   ├── core/
│   │   ├── StringPool.{h,cpp}   – interned string storage (32-bit IDs)
│   │   └── Graph.{h,cpp}        – flat-array node/edge storage
│   └── layout/
│       └── GraphvizLayout.{h,cpp} – sfdp layout wrapper
├── parser/
│   └── ClangParser.{h,cpp}      – libclang #include extractor
└── ui/
    ├── MainWindow.{h,cpp}        – top-level window, menus, side panel
    ├── GraphView.{h,cpp}         – QGraphicsView with pan/zoom
    ├── GraphScene.{h,cpp}        – QGraphicsScene: populate, filter, highlight
    ├── NodeItem.{h,cpp}          – rounded-rect node with LOD
    ├── EdgeItem.{h,cpp}          – directed bezier edge with arrowhead
    └── EdgeLabelItem.{h,cpp}     – mid-edge label box
```

---

## Performance Notes

* The sfdp layout is synchronous and runs on the main thread. For graphs with
  >10 000 nodes it may take tens of seconds; progress is logged to stdout via
  spdlog.
* Qt's BSP tree index is used for fast spatial queries in the scene.
* LOD thresholds: `lod < 0.15` → dot; `lod < 0.35` → plain rect (no text or
  labels); `lod ≥ 0.35` → full rendering.
