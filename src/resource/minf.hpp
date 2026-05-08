#pragma once

#include <cstdint>

namespace resource {

struct ResTimeSignature {
    std::uint16_t upper, lower;
};

struct ResTempoMeter {
    std::uint32_t samplePos;
    float tempo;
    ResTimeSignature timeSignature;
};

struct ResChordOffset {
    std::uint32_t samplePos;
    std::uint32_t offset;
};

struct ResBeat {
    std::uint32_t samplePos;
    std::uint32_t beatNum;
};

struct ResTable {
    std::uint16_t count;
    std::int16_t loopBaseIndex;
};

struct ResPointMarker {
    std::uint32_t nameOffset;
    ResTable samplePositions;
};

struct ResRangedMarkerPoint {
    std::uint32_t samplePos;
    std::uint32_t _04;
};

struct ResRangedMarkerRange {
    std::uint32_t nameOffset;
    std::uint32_t endOffset;
};

struct ResMarkerOffset {
    std::uint32_t hash;
    std::uint32_t offset;
};

struct ResMusicInfo {
    std::uint32_t magic;
    std::uint16_t bom;
    std::uint16_t version;
    std::uint32_t size;
    std::uint32_t enNameOffset;
    std::uint32_t jpNameOffset;
    std::uint32_t sampleRate;
    std::uint32_t loopStart;
    std::uint32_t sampleCount;
    ResTempoMeter defaultTempoMeter;
    std::uint32_t tempoOffset;
    std::uint32_t chordOffset;
    std::uint32_t _34;
    std::uint32_t beatOffset;
    std::uint32_t measureOffset;
    std::uint32_t pointMarkerOffset;
    std::uint32_t rangedMarkerOffset;
    std::uint32_t sequenceOffset;
};

} // namespace resource