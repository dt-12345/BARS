#pragma once

#include "common/endian.hpp"

#include <cstdint>

namespace resource {

struct ResAssetOffset {
    std::uint32_t metaOffset;
    std::uint32_t dataOffset;

    auto byteswap() const -> ResAssetOffset {
        return {
            .metaOffset = common::ByteSwap(metaOffset),
            .dataOffset = common::ByteSwap(dataOffset),
        };
    }
};

struct ResAudioResource {
    static const constexpr std::uint32_t cMagic = 0x53524142; // BARS
    
    std::uint32_t magic;
    std::uint32_t fileSize;
    std::uint16_t bom;
    std::uint16_t version;
    std::uint32_t assetCount;
};

} // namespace resource