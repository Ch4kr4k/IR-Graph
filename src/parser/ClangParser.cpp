#include "ClangParser.h"

#include <clang-c/Index.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Visitor callback
// ---------------------------------------------------------------------------
struct VisitorCtx {
    Graph*       graph;
    std::string  fromPath; ///< The file being parsed (outer TU file)
};

static CXChildVisitResult includeVisitor(CXCursor cursor,
                                         CXCursor /*parent*/,
                                         CXClientData data) {
    if (clang_getCursorKind(cursor) != CXCursor_InclusionDirective)
        return CXChildVisit_Continue;

    auto* ctx = static_cast<VisitorCtx*>(data);

    // ── File that contains this #include ──────────────────────────────────
    CXSourceLocation loc = clang_getCursorLocation(cursor);
    CXFile           fromFile{};
    unsigned         line{};
    clang_getExpansionLocation(loc, &fromFile, &line, nullptr, nullptr);

    CXString fromFileStr = clang_getFileName(fromFile);
    std::string fromPath(clang_getCString(fromFileStr) ? clang_getCString(fromFileStr) : "");
    clang_disposeString(fromFileStr);

    // ── Included file ─────────────────────────────────────────────────────
    CXFile includedFile = clang_getIncludedFile(cursor);
    if (!includedFile) return CXChildVisit_Continue; // unresolved

    CXString includedStr = clang_getFileName(includedFile);
    std::string includedPath(clang_getCString(includedStr) ? clang_getCString(includedStr) : "");
    clang_disposeString(includedStr);

    if (fromPath.empty() || includedPath.empty()) return CXChildVisit_Continue;

    // Resolve to canonical paths to avoid duplicates from symlinks
    std::error_code ec;
    auto fromCanon     = fs::weakly_canonical(fromPath, ec);
    auto includedCanon = fs::weakly_canonical(includedPath, ec);
    if (ec) return CXChildVisit_Continue;

    // ── Detect include type (<angled> vs "quoted") ────────────────────────
    // Heuristic: system headers are under /usr or /lib.
    const std::string& ip = includedCanon.string();
    bool isAngled = ip.starts_with("/usr/") || ip.starts_with("/lib/")
                 || ip.starts_with("/opt/");

    Graph& g = *ctx->graph;
    NodeId fromId = g.addNode(fromCanon.string());
    NodeId toId   = g.addNode(includedCanon.string());
    g.addEdge(fromId, toId, line, isAngled);

    return CXChildVisit_Continue;
}

// ---------------------------------------------------------------------------
ClangParser::ClangParser() {
    // excludeDeclarationsFromPCH=0, displayDiagnostics=0
    index_ = clang_createIndex(0, 0);
}

ClangParser::~ClangParser() {
    if (index_) clang_disposeIndex(static_cast<CXIndex>(index_));
}

void ClangParser::addCompilerFlag(const std::string& flag) {
    extraFlags_.push_back(flag);
}

bool ClangParser::parseDirectory(const fs::path& dir, Graph& graph) {
    static const std::array<std::string, 8> kExts{
        ".c", ".cpp", ".cxx", ".cc", ".C", ".h", ".hpp", ".hxx"
    };

    std::vector<std::string> files;
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        if (std::find(kExts.begin(), kExts.end(), ext) != kExts.end())
            files.push_back(entry.path().string());
    }
    if (ec) {
        spdlog::error("ClangParser: directory iteration error: {}", ec.message());
        return false;
    }
    spdlog::info("ClangParser: found {} source files in {}", files.size(), dir.string());
    return parseFiles(files, graph);
}

bool ClangParser::parseFiles(const std::vector<std::string>& files, Graph& graph) {
    std::size_t ok = 0;
    for (const std::string& f : files) {
        if (parseFile(f, graph)) ++ok;
    }
    spdlog::info("ClangParser: parsed {}/{} files.  {} nodes, {} edges.",
                 ok, files.size(), graph.nodeCount(), graph.edgeCount());
    return ok > 0 || files.empty();
}

bool ClangParser::parseFile(const std::string& filePath, Graph& graph) {
    // Build argv for libclang
    std::vector<const char*> args;
    for (const auto& f : extraFlags_) args.push_back(f.c_str());

    // CXTranslationUnit_SkipFunctionBodies for speed
    unsigned tuFlags = CXTranslationUnit_SkipFunctionBodies
                     | CXTranslationUnit_DetailedPreprocessingRecord
                     | CXTranslationUnit_KeepGoing;

    CXTranslationUnit tu = clang_parseTranslationUnit(
        static_cast<CXIndex>(index_),
        filePath.c_str(),
        args.empty() ? nullptr : args.data(),
        static_cast<int>(args.size()),
        nullptr, 0,
        tuFlags
    );

    if (!tu) {
        spdlog::warn("ClangParser: failed to parse '{}'", filePath);
        return false;
    }

    VisitorCtx ctx{ &graph, filePath };
    CXCursor   root = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(root, includeVisitor, &ctx);

    clang_disposeTranslationUnit(tu);
    return true;
}
