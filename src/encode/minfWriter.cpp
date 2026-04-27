#include "common/hash.hpp"
#include "encode/minfWriter.hpp"
#include "common/writer.hpp"
#include "resource/minf.hpp"

#include <filesystem>
#include <fstream>

namespace encode {

auto MinfWriter::WriteRelativeOffset(common::BinaryWriter& writer, std::uint32_t offsetValue, std::uint32_t offsetBase, bool updatePos) -> void {
    writer.writeAt(offsetValue - offsetBase, offsetBase);
    if (updatePos) {
        writer.seek(offsetBase + sizeof(std::uint32_t));
    }
}

auto MinfWriter::WriteTempoMeter(const sound::TempoMeter& tempoMeter, common::BinaryWriter& writer) -> bool {
    writer.write(tempoMeter.samplePos);
    writer.write(tempoMeter.tempo);
    writer.write(tempoMeter.timeSignature.upper);
    writer.write(tempoMeter.timeSignature.lower);
    return true;
}

auto MinfWriter::WriteBeat(const sound::Beat& beat, common::BinaryWriter& writer) -> bool {
    writer.write(beat.samplePos);
    writer.write(beat.beatNum);
    return true;
}

auto MinfWriter::WritePointMarker(const sound::PointMarker& marker, common::BinaryWriter& writer, std::map<std::uint32_t, std::string>& strings) -> bool {
    strings.emplace(writer.tell(), marker.name);
    writer.skip(4);
    writer.write(static_cast<std::uint16_t>(marker.samplePositions.size()));
    writer.write(marker.loopBaseIndex);
    for (const auto pos : marker.samplePositions) {
        writer.write(pos);
    }
    return true;
}

auto MinfWriter::WriteRangeMarker(const sound::RangeMarker& marker, common::BinaryWriter& writer, std::map<std::uint32_t, std::string>& strings) -> bool {
    strings.emplace(writer.tell(), marker.name);
    writer.skip(4);
    const auto startPointSize = sizeof(resource::ResTable) + marker.starts.entries.size() * sizeof(resource::ResRangedMarkerPoint);
    writer.write(static_cast<std::uint32_t>(sizeof(std::uint32_t) + startPointSize)); // offset to end points is sizeof(endOffset) + sizeof(StartMarkerPoints)
    writer.write(static_cast<std::uint16_t>(marker.starts.entries.size()));
    writer.write(marker.starts.loopBaseIndex);
    for (const auto& pos : marker.starts.entries) {
        writer.write(pos.samplePos);
        writer.write(pos._04);
    }
    writer.write(static_cast<std::uint16_t>(marker.ends.entries.size()));
    writer.write(marker.ends.loopBaseIndex);
    for (const auto& pos : marker.ends.entries) {
        writer.write(pos.samplePos);
        writer.write(pos._04);
    }
    return true;
}

auto MinfWriter::Write(const sound::MusicInfo& info, common::BinaryWriter& writer) -> bool {
    writer.setEndian(info.getEndian());
    const auto base = writer.tell();

    auto stringOffsets = std::map<std::uint32_t, std::string>{};

    writer.writeArrayFixed<char, 4>({ 'M', 'I', 'N', 'F' });
    writer.write(static_cast<std::uint16_t>(0xfeff));
    writer.write(static_cast<std::uint16_t>(0x101));
    writer.skip(4); // file size, fill in later
    stringOffsets.emplace(writer.tell(), info.getTrackName());
    writer.skip(4);
    stringOffsets.emplace(writer.tell(), info.getJapaneseName());
    writer.skip(4);
    writer.write(info.getSampleRate());
    writer.write(info.getLoopStart());
    writer.write(info.getSampleCount());
    if (!WriteTempoMeter(info.getDefaultTempoMeter(), writer)) {
        return false;
    }
    writer.skip(0x20); // section offsets, fill in later

    if (info.getTempoMeterTable()) {
        const auto offset = writer.tell();
        WriteRelativeOffset(writer, offset, base + offsetof(resource::ResMusicInfo, tempoOffset));

        const auto& tempoMeterTable = *info.getTempoMeterTable();
        writer.write(static_cast<std::uint16_t>(tempoMeterTable.entries.size()));
        writer.write(tempoMeterTable.loopBaseIndex);
        for (const auto& tempoMeter : tempoMeterTable.entries) {
            if (!WriteTempoMeter(tempoMeter, writer)) {
                return false;
            }
        }
    }

    if (info.getBeatTable()) {
        const auto offset = writer.tell();
        WriteRelativeOffset(writer, offset, base + offsetof(resource::ResMusicInfo, beatOffset));

        const auto& beatTable = *info.getBeatTable();
        writer.write(static_cast<std::uint16_t>(beatTable.entries.size()));
        writer.write(beatTable.loopBaseIndex);
        for (const auto& beat : beatTable.entries) {
            if (!WriteBeat(beat, writer)) {
                return false;
            }
        }
    }

    if (info.getMeasureTable()) {
        const auto offset = writer.tell();
        WriteRelativeOffset(writer, offset, base + offsetof(resource::ResMusicInfo, measureOffset));

        const auto& measureTable = *info.getMeasureTable();
        writer.write(static_cast<std::uint16_t>(measureTable.entries.size()));
        writer.write(measureTable.loopBaseIndex);
        for (const auto& measure : measureTable.entries) {
            writer.write(measure);
        }
    }

    if (info.getPointMarkerTable()) {
        const auto offset = writer.tell();
        WriteRelativeOffset(writer, offset, base + offsetof(resource::ResMusicInfo, pointMarkerOffset));

        const auto& pointMarkerTable = info.getPointMarkerTable();
        auto sortedMarkers = std::map<std::uint32_t, std::reference_wrapper<const sound::PointMarker>>{};
        std::transform(
            pointMarkerTable->begin(), pointMarkerTable->end(), std::inserter(sortedMarkers, sortedMarkers.begin()),
            [](const auto& marker) { return std::make_pair(common::CalcMmh3(marker.name), std::cref(marker)); }
        );
        writer.write(static_cast<std::uint32_t>(sortedMarkers.size()));
        auto markerOffset = writer.tell() + sortedMarkers.size() * sizeof(resource::ResMarkerOffset);
        for (const auto& [hash, marker] : sortedMarkers) {
            writer.write(hash);
            WriteRelativeOffset(writer, markerOffset, writer.tell(), true);
            markerOffset += sizeof(resource::ResPointMarker) + marker.get().samplePositions.size() * sizeof(std::uint32_t);
        }
        for (const auto& [_, marker] : sortedMarkers) {
            if (!WritePointMarker(marker, writer, stringOffsets)) {
                return false;
            }
        }
    }

    if (info.getRangeMarkerTable()) {
        const auto offset = writer.tell();
        WriteRelativeOffset(writer, offset, base + offsetof(resource::ResMusicInfo, rangedMarkerOffset));

        const auto& rangeMarkerTable = info.getRangeMarkerTable();
        auto sortedMarkers = std::map<std::uint32_t, std::reference_wrapper<const sound::RangeMarker>>{};
        std::transform(
            rangeMarkerTable->begin(), rangeMarkerTable->end(), std::inserter(sortedMarkers, sortedMarkers.begin()),
            [](const auto& marker) { return std::make_pair(common::CalcMmh3(marker.name), std::cref(marker)); }
        );
        writer.write(static_cast<std::uint32_t>(sortedMarkers.size()));
        auto markerOffset = writer.tell() + sortedMarkers.size() * sizeof(resource::ResMarkerOffset);
        for (const auto& [hash, marker] : sortedMarkers) {
            writer.write(hash);
            WriteRelativeOffset(writer, markerOffset, writer.tell(), true);
            markerOffset += sizeof(resource::ResRangedMarkerRange)
                         +  sizeof(resource::ResTable)
                         +  marker.get().starts.entries.size() * sizeof(resource::ResRangedMarkerPoint)
                         +  sizeof(resource::ResTable)
                         +  marker.get().ends.entries.size() * sizeof(resource::ResRangedMarkerPoint);
        }
        for (const auto& [_, marker] : sortedMarkers) {
            if (!WriteRangeMarker(marker, writer, stringOffsets)) {
                return false;
            }
        }
    }

    auto unique = std::map<std::string, std::uint32_t>{};
    for (const auto& [_, str] : stringOffsets) {
        unique.emplace(str, 0);
    }

    for (auto& [str, offset] : unique) {
        offset = writer.tell();
        writer.writeString(str);
    }

    for (const auto& [offset, str] : stringOffsets) {
        WriteRelativeOffset(writer, unique.at(str), offset);
    }

    const auto size = static_cast<std::uint32_t>(writer.tell() - base);
    writer.writeAt(size, base + offsetof(resource::ResMusicInfo, size));
    return true;
}

auto MinfWriter::Write(const sound::MusicInfo& info, std::vector<std::uint8_t>& buffer) -> bool {
    auto writer = common::BinaryWriter();
    if (!Write(info, writer)) {
        return false;
    }
    buffer.swap(writer.buffer());
    return true;
}

auto MinfWriter::Write(const sound::MusicInfo& info, std::string_view path) -> bool {
    auto buffer = std::vector<std::uint8_t>{};
    if (!Write(info, buffer)) {
        return false;
    }

    const auto fpath = std::filesystem::path(path);
    if (!fpath.parent_path().empty()) {
        std::filesystem::create_directories(fpath.parent_path());
    }

    auto outfile = std::ofstream(fpath, std::ios::binary);
    if (!outfile) {
        return false;
    }

    outfile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return true;
}

} // namespace encode