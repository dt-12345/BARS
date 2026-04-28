#pragma once

#include <cstdint>

namespace resource {
    
enum class AttenuationChannel : std::uint8_t {
    Left = 0,
    Right = 1,
    LeftSurround = 2,
    RightSurround = 3,
    Center = 4,
    LowFrequencyEffect = 5,
};

struct ResTrackChannel {
    std::uint8_t inputChannelIndex;
    AttenuationChannel outputChannel;
};

enum class MetaFlags : std::uint32_t {
    IsStreaming = 0,
    HasLocalSamples = 1,
    IsLoop = 2,
    _03 = 3,
    IsOpus = 4,
    _05 = 5,
    IsOpusDecodeFourFramesAtOnce = 6,
    IsUniformAttenuationPerTrack = 7,
};

enum class OptFlags : std::uint32_t {
    MaxAmplitude = 0,
    _01 = 1,
    MaxMomentaryLufs = 2,
    IntegratedLufs = 3,
    TailLength = 4,
    _05 = 5,
    _06 = 6,
    _07 = 7,
};

struct ResPoint {
    std::uint32_t samplePos;
    float _04;
};

struct ResMarker {
    std::uint32_t id;
    std::uint32_t nameOffset;
    std::uint32_t samplePos;
    std::uint32_t duration;
};

struct ResAttribute {
    std::uint32_t keyOffset;
    std::uint32_t value;
};

struct __attribute__((packed)) ResAudioMetadata {
    std::uint32_t magic;
    std::uint16_t bom;
    std::uint16_t version;
    std::uint32_t size;
    std::uint32_t _0c;
    std::uint32_t dataOffset;
    std::uint32_t markerOffset;
    std::uint32_t musicInfoOffset;
    std::uint32_t tagOffset;
    std::uint32_t attrOffset;
    std::uint32_t nameOffset;
    std::uint32_t hash;
    std::uint8_t metaFlags;
    std::uint8_t trackCount;
};

} // namespace resource