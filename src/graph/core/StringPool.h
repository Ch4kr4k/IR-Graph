#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/// Interns unique strings and returns stable 32-bit IDs.
class StringPool {
public:
    using StringId = uint32_t;
    static constexpr StringId INVALID = UINT32_MAX;

    StringId intern(std::string_view s);
    std::string_view get(StringId id) const;
    std::size_t size() const { return strings_.size(); }

private:
    std::vector<std::string>             strings_;
    std::unordered_map<std::string, StringId> index_;
};
