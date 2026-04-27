#pragma once

#include "sound/asset.hpp"

#include <list>

namespace sound {

class Archive {
public:
    Archive() : mAssets(), mEndian(std::endian::native) {}

    [[nodiscard]] auto getAsset(std::string_view name) -> Asset*;
    [[nodiscard]] auto getAsset(std::string_view name) const -> const Asset*;

    [[nodiscard]] auto getAssets() -> std::list<Asset>& { return mAssets; }
    [[nodiscard]] auto getAssets() const -> const std::list<Asset>& { return mAssets; }

    template <typename... Args>
    auto addAsset(Args&&... args) -> Asset& { auto& asset = mAssets.emplace_back(std::forward<Args>(args)...); asset.setEndian(mEndian); return asset; }

    auto setEndian(std::endian endian) -> void { mEndian = endian; }
    [[nodiscard]] auto getEndian() const -> std::endian { return mEndian; }

private:
    std::list<Asset> mAssets;
    std::endian mEndian;
};

} // namespace sound