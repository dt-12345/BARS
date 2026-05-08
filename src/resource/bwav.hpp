#pragma once

#include "resource/adpcm.hpp"

namespace resource {

enum class SampleFormat : std::uint16_t {
    PcmInt16 = 0,
    Adpcm = 1,
    Opus = 2,
};

enum class OutputChannel : std::uint16_t {
    Left = 0,
    Right = 1,
    Center = 2,
    LowFrequencyEffect = 3,
    LeftSurround = 4,
    RightSurround = 5,
};

enum class AssetType : std::uint16_t {
    Normal = 0,
    Prefetch = 1,
    Invalid = 0xff,
};

struct OpusParameter {
    std::uint32_t sampleCount; // overlapping sample count
    std::uint32_t offset;
    std::uint8_t padding[0x18];
};

struct ResSeekPoint {
    std::uint32_t samplePos;
    AdpcmContext loopCtx;
    std::uint16_t padding;
};
static_assert(sizeof(ResSeekPoint) == 0xc);

struct ResChannelInfo {
    SampleFormat format;
    OutputChannel outputChannel;
    std::uint32_t sampleRate;
    std::uint32_t totalSampleCount;
    std::uint32_t localSampleCount;
    union {
        std::uint8_t padding[0x20];
        AdpcmParameter adpcm;
        OpusParameter opus;
    } parameter;
    std::uint32_t totalSamplesOffset;
    std::uint32_t localSamplesOffset;
    std::uint16_t loopCount;
    std::uint16_t loopSeekPointIndex;
    std::int32_t loopEnd;
};
static_assert(sizeof(ResChannelInfo) == 0x40);

struct ResBinaryWaveform {
    std::uint32_t magic;
    std::uint16_t bom;
    std::uint16_t version;
    std::uint32_t dataHash;
    AssetType assetType;
    std::uint16_t channelCount;

    static constexpr const auto cVersion = std::uint16_t(1);
};

} // namespace resource