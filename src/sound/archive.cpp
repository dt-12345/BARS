#include "sound/archive.hpp"

#include <algorithm>

namespace sound {

auto Archive::getAsset(std::string_view name) -> Asset* {
    const auto res = std::find_if(mAssets.begin(), mAssets.end(), [&](const auto& asset) { return asset.getName() == name; });
    if (res == mAssets.end()) {
        return nullptr;
    }
    return std::addressof(*res);
}

auto Archive::getAsset(std::string_view name) const -> const Asset* {
    const auto res = std::find_if(mAssets.begin(), mAssets.end(), [&](const auto& asset) { return asset.getName() == name; });
    if (res == mAssets.end()) {
        return nullptr;
    }
    return std::addressof(*res);
}
    
} // namespace sound