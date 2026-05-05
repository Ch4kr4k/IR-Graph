#pragma once
#include "graph/core/Graph.h"
#include <filesystem>
#include <string>
#include <vector>

/// Parses C/C++ source files with libclang and populates a Graph with
/// #include dependency edges.
class ClangParser {
public:
    ClangParser();
    ~ClangParser();

    // Non-copyable
    ClangParser(const ClangParser&)            = delete;
    ClangParser& operator=(const ClangParser&) = delete;

    /// Recursively find every .c .cpp .h .hpp .cxx .cc file under @p dir
    /// and parse them.  Returns false on fatal error.
    bool parseDirectory(const std::filesystem::path& dir, Graph& graph);

    /// Parse a hand-picked list of files.
    bool parseFiles(const std::vector<std::string>& files, Graph& graph);

    /// Extra compiler flags (e.g. include paths) forwarded to libclang.
    void addCompilerFlag(const std::string& flag);

private:
    bool parseFile(const std::string& filePath, Graph& graph);

    void*                    index_{nullptr}; ///< CXIndex (opaque, avoids leaking clang-c headers)
    std::vector<std::string> extraFlags_;
};
