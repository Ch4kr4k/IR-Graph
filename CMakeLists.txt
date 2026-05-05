cmake_minimum_required(VERSION 3.20)
project(IR_Graph
    VERSION 1.0.0
    DESCRIPTION "C/C++ #include dependency graph visualizer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Portability: add /usr/lib64 prefix for Gentoo and other multilib distros ──
foreach(_lib64 /usr/lib64 /usr/local/lib64)
    if(EXISTS "${_lib64}/cmake" AND NOT "${_lib64}" IN_LIST CMAKE_PREFIX_PATH)
        list(APPEND CMAKE_PREFIX_PATH "${_lib64}")
    endif()
endforeach()

# ── Qt6 ────────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
qt_standard_project_setup()   # sets AUTOMOC/AUTOUIC/AUTORCC ON

# ── spdlog ─────────────────────────────────────────────────────────────────────
find_package(spdlog REQUIRED)

# ── Graphviz ───────────────────────────────────────────────────────────────────
# Priority: vcpkg → pkg-config → direct find_library (Gentoo / no pkg-config).
find_package(unofficial-graphviz CONFIG QUIET)
if(unofficial-graphviz_FOUND)
    message(STATUS "Found Graphviz via vcpkg")
    set(GRAPHVIZ_TARGETS unofficial::graphviz::cgraph unofficial::graphviz::gvc)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GRAPHVIZ QUIET libcgraph libgvc)
    endif()

    if(GRAPHVIZ_FOUND)
        add_library(gv_iface INTERFACE)
        target_include_directories(gv_iface INTERFACE ${GRAPHVIZ_INCLUDE_DIRS})
        target_link_libraries(gv_iface INTERFACE ${GRAPHVIZ_LINK_LIBRARIES})
        target_link_options(gv_iface INTERFACE ${GRAPHVIZ_LDFLAGS_OTHER})
        set(GRAPHVIZ_TARGETS gv_iface)
        message(STATUS "Found Graphviz via pkg-config")
    else()
        # Fallback: direct search (Gentoo, Arch, etc.)
        find_library(GVC_LIB    NAMES gvc    PATH_SUFFIXES graphviz)
        find_library(CGRAPH_LIB NAMES cgraph PATH_SUFFIXES graphviz)
        find_path(GRAPHVIZ_INCDIR NAMES graphviz/gvc.h gvc.h
            PATH_SUFFIXES graphviz include/graphviz)
        if(NOT GVC_LIB OR NOT CGRAPH_LIB OR NOT GRAPHVIZ_INCDIR)
            message(FATAL_ERROR
                "Graphviz (libgvc + libcgraph) not found.\n"
                "  Gentoo : sudo emerge media-gfx/graphviz\n"
                "  Ubuntu : sudo apt install libgraphviz-dev\n"
                "  Fedora : sudo dnf install graphviz-devel")
        endif()
        add_library(gv_iface INTERFACE)
        target_include_directories(gv_iface INTERFACE ${GRAPHVIZ_INCDIR})
        target_link_libraries(gv_iface INTERFACE ${GVC_LIB} ${CGRAPH_LIB})
        set(GRAPHVIZ_TARGETS gv_iface)
        message(STATUS "Found Graphviz via find_library: ${GVC_LIB}")
    endif()
endif()

# ── libclang ──────────────────────────────────────────────────────────────────
# Priority: ClangConfig.cmake → llvm-config → direct find_library.
find_package(Clang CONFIG QUIET)
if(Clang_FOUND AND TARGET libclang)
    message(STATUS "Found libclang via ClangConfig.cmake (LLVM ${LLVM_PACKAGE_VERSION})")
    set(CLANG_TARGET libclang)
    set(CLANG_INCLUDES ${CLANG_INCLUDE_DIRS})
else()
    # Use llvm-config from the highest-version slot available
    find_program(LLVM_CONFIG_EXE
        NAMES llvm-config
              llvm-config-21 llvm-config-20 llvm-config-19 llvm-config-18
              llvm-config-17 llvm-config-16 llvm-config-15 llvm-config-14
        HINTS /usr/lib/llvm/21/bin /usr/lib/llvm/20/bin /usr/lib/llvm/19/bin
              /usr/lib/llvm/18/bin
    )
    if(LLVM_CONFIG_EXE)
        execute_process(COMMAND ${LLVM_CONFIG_EXE} --libdir
            OUTPUT_VARIABLE _LLVM_LIBDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${LLVM_CONFIG_EXE} --includedir
            OUTPUT_VARIABLE _LLVM_INCDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
        message(STATUS "llvm-config: libdir=${_LLVM_LIBDIR}, incdir=${_LLVM_INCDIR}")
    endif()

    find_library(LIBCLANG_LIB
        NAMES clang libclang libclang.lib
        HINTS ${_LLVM_LIBDIR}
              # Gentoo multi-slot layout: /usr/lib/llvm/<ver>/lib64
              /usr/lib/llvm/21/lib64
              /usr/lib/llvm/20/lib64
              /usr/lib/llvm/19/lib64
              /usr/lib/llvm/18/lib64
              # Debian/Ubuntu
              /usr/lib/x86_64-linux-gnu
              /usr/lib/aarch64-linux-gnu
              # Windows LLVM default install
              "C:/Program Files/LLVM/lib"
              "C:/LLVM/lib"
        PATH_SUFFIXES llvm-18/lib llvm-17/lib llvm-16/lib llvm-15/lib llvm-14/lib
    )
    find_path(LIBCLANG_INCDIR clang-c/Index.h
        HINTS ${_LLVM_INCDIR}
              /usr/lib/llvm/21/include
              /usr/lib/llvm/20/include
              /usr/lib/llvm/19/include
              /usr/lib/llvm/18/include
              /usr/include
              "C:/Program Files/LLVM/include"
              "C:/LLVM/include"
        PATH_SUFFIXES llvm-18 llvm-17 llvm-16 llvm-15 llvm-14
    )

    if(NOT LIBCLANG_LIB OR NOT LIBCLANG_INCDIR)
        message(FATAL_ERROR
            "libclang not found.\n"
            "  Gentoo : emerge llvm-core/llvm (already installed if LLVM is present)\n"
            "  Ubuntu : sudo apt install libclang-dev\n"
            "  Fedora : sudo dnf install clang-devel")
    endif()

    add_library(clang_iface INTERFACE)
    target_include_directories(clang_iface INTERFACE ${LIBCLANG_INCDIR})
    target_link_libraries(clang_iface INTERFACE ${LIBCLANG_LIB})
    set(CLANG_TARGET clang_iface)
    set(CLANG_INCLUDES ${LIBCLANG_INCDIR})
    message(STATUS "Found libclang: ${LIBCLANG_LIB}")
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/graph/core/StringPool.cpp
    src/graph/core/Graph.cpp
    src/graph/layout/GraphvizLayout.cpp
    src/parser/ClangParser.cpp
    src/ui/MainWindow.cpp
    src/ui/GraphView.cpp
    src/ui/GraphScene.cpp
    src/ui/NodeItem.cpp
    src/ui/EdgeLabelItem.cpp
    src/ui/EdgeItem.cpp
)

if(WIN32)
    qt_add_executable(IR_Graph WIN32 ${SOURCES})
else()
    qt_add_executable(IR_Graph ${SOURCES})
endif()

target_include_directories(IR_Graph PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CLANG_INCLUDES}
)

target_link_libraries(IR_Graph PRIVATE
    Qt6::Widgets
    Qt6::Core
    spdlog::spdlog
    ${GRAPHVIZ_TARGETS}
    ${CLANG_TARGET}
)

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(IR_Graph PRIVATE -Wall -Wextra -Wno-unused-parameter)
elseif(MSVC)
    target_compile_options(IR_Graph PRIVATE
        /W3
        /permissive-
        /utf-8
        /wd4100   # unreferenced formal parameter
        /wd4244   # conversion, possible loss of data
        /wd4267   # size_t narrowing
    )
    target_compile_definitions(IR_Graph PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

install(TARGETS IR_Graph RUNTIME DESTINATION bin)
