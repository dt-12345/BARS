#pragma once

#include "common/writer.hpp"
#include "sound/music.hpp"

#include <map>

namespace encode {
    
class MinfWriter {
public:
    static auto Write(const sound::MusicInfo& info, common::BinaryWriter& writer) -> bool;
    static auto Write(const sound::MusicInfo& info, std::vector<std::uint8_t>& buffer) -> bool;
    static auto Write(const sound::MusicInfo& info, std::string_view path) -> bool;

private:
    static auto WriteRelativeOffset(common::BinaryWriter& writer, std::uint32_t offsetValue, std::uint32_t offsetBase, bool updatePos = false) -> void;
    static auto WriteTempoMeter(const sound::TempoMeter& tempoMeter, common::BinaryWriter& writer) -> bool;
    static auto WriteBeat(const sound::Beat& beat, common::BinaryWriter& writer) -> bool;
    static auto WritePointMarker(const sound::PointMarker& marker, common::BinaryWriter& writer, std::map<std::uint32_t, std::string>& strings) -> bool;
    static auto WriteRangeMarker(const sound::RangeMarker& marker, common::BinaryWriter& writer, std::map<std::uint32_t, std::string>& strings) -> bool;
};

} // namespace encode