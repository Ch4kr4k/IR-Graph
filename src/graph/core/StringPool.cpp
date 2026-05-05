#include "StringPool.h"

StringPool::StringId StringPool::intern(std::string_view s) {
    std::string key(s);
    auto it = index_.find(key);
    if (it != index_.end()) return it->second;
    StringId id = static_cast<StringId>(strings_.size());
    strings_.push_back(key);
    index_[key] = id;
    return id;
}

std::string_view StringPool::get(StringId id) const {
    if (id >= strings_.size()) return {};
    return strings_[id];
}
