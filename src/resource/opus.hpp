#pragma once

#include "common/endian.hpp"

#include <algorithm>
#include <array>

namespace resource {

constexpr const auto cOpusHeaderVersion = std::uint8_t(0);
constexpr const auto cOpusFrameDuration = 20u;
constexpr const auto cOpusSamplesPerFrame = 960u;

static constexpr const auto cValidOpusSampleRates = std::array{
    8000u, 16000u, 24000u, 48000u,
};

inline constexpr auto IsValidOpusSampleRate(std::uint32_t rate) -> bool {
    return std::find(cValidOpusSampleRates.begin(), cValidOpusSampleRates.end(), rate) != cValidOpusSampleRates.end();
}

enum class ChunkType : std::uint32_t {
    Header  = 0x80000001,
    _02     = 0x80000002,
    Context = 0x80000003,
    Data    = 0x80000004,
};

// nn::codec::detail::OpusPacketInternal::Header
struct OpusPacket {
    std::uint32_t size; // big endian
    std::uint32_t finalRange; // big endian
    // data

    auto getSize() const -> std::uint32_t {
        return common::ByteSwapIfNeeded(size, std::endian::big);
    }

    // final encoder state
    auto getFinalRange() const -> std::uint32_t {
        return common::ByteSwapIfNeeded(finalRange, std::endian::big);
    }

    auto setSize(std::uint32_t _size) -> void {
        size = common::ByteSwapIfNeeded(_size, std::endian::big);
    }

    auto setFinalRange(std::uint32_t _finalRange) -> void {
        finalRange = common::ByteSwapIfNeeded(_finalRange, std::endian::big);
    }
};

struct OpusHeader {
    std::uint8_t version;
    std::uint8_t channelCount; // must be 1 for BARS
    std::uint16_t frameSize; // 0 if variable
    std::uint32_t sampleRate;
    std::uint32_t dataOffset;
    std::uint32_t _02Offset;
    std::uint32_t contextOffset;
    std::uint16_t delayCompensation;
    std::uint16_t reserved; // padding
};

struct OpusChunk {
    ChunkType type;
    std::uint32_t size;
};

} // namespace resource