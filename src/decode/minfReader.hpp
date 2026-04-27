#pragma once

#include "common/reader.hpp"
#include "decode/decodeResult.hpp"
#include "sound/music.hpp"

namespace decode {

class MinfReader {
public:
    [[nodiscard]] static auto Read(common::BinaryReader& reader, sound::MusicInfo& info) -> Result;

private:
    [[nodiscard]] static auto ReadTempoMeter(common::BinaryReader& reader, sound::TempoMeter& tempoMeter) -> Result;
    [[nodiscard]] static auto ReadBeat(common::BinaryReader& reader, sound::Beat& beat) -> Result;
    [[nodiscard]] static auto ReadPointMarker(common::BinaryReader& reader, sound::PointMarker& marker) -> Result;
    [[nodiscard]] static auto ReadRangeMarker(common::BinaryReader& reader, sound::RangeMarker& marker) -> Result;

    [[nodiscard]] static auto ReadTempoTable(common::BinaryReader& reader, sound::Table<sound::TempoMeter>& table) -> Result;
    [[nodiscard]] static auto ReadBeatTable(common::BinaryReader& reader, sound::Table<sound::Beat>& table) -> Result;
    [[nodiscard]] static auto ReadMeasureTable(common::BinaryReader& reader, sound::Table<std::uint32_t>& table) -> Result;
    [[nodiscard]] static auto ReadPointMarkerTable(common::BinaryReader& reader, std::vector<sound::PointMarker>& table) -> Result;
    [[nodiscard]] static auto ReadRangeMarkerTable(common::BinaryReader& reader, std::vector<sound::RangeMarker>& table) -> Result;
};

} // namespace decode