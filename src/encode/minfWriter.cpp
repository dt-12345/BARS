#include "common/hash.hpp"
#include "encode/minfWriter.hpp"
#include "common/writer.hpp"
#include "resource/minf.hpp"
#include "sound/music.hpp"

#include <filesystem>
#include <fstream>
#include <ranges>

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

auto MinfWriter::WriteChord(const sound::Chord& chord, common::BinaryWriter& writer) -> bool {
    writer.write(static_cast<std::uint32_t>(chord.notes.size()));
    writer.writeArray(std::span<const sound::Note>{ chord.notes });
    writer.alignUp(4);
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

auto MinfWriter::WriteS3SequenceTable(const sound::thunder::SequenceTable& table, common::BinaryWriter& writer) -> bool {
    writer.write(1u);
    const auto base = writer.tell();
    writer.skip(4);

    WriteRelativeOffset(writer, writer.tell(), base);
    writer.write(static_cast<std::uint32_t>(table.size()));
    for (const auto& track : table) {
        writer.write(static_cast<std::uint32_t>(track.name.size()));
        writer.writeArray(std::span<const char>{ track.name.data(), track.name.size() });
        writer.write(static_cast<std::uint32_t>(track.notes.size()));
        for (const auto& note : track.notes) {
            writer.write(note.start);
            writer.write(note.end);
            writer.write(note.pitch);
            writer.write(note.velocity);
            writer.write(note._0c);
        }
        writer.write(static_cast<std::uint32_t>(track._02.size()));
        for (const auto& entry : track._02) {
            writer.write(entry._00);
            writer.write(entry._04);
        }
        writer.write(static_cast<std::uint32_t>(track._03.size()));
        for (const auto& entry : track._03) {
            writer.write(entry.samplePosition);
            writer.write(entry._04);
        }
    }

    return true;
}

auto MinfWriter::WriteACNHSequenceTable(const sound::park::GenericMusic& table, common::BinaryWriter& writer) -> bool {
    writer.write(4u);
    const auto base = writer.tell();
    writer.seek(base + 4 * sizeof(std::uint32_t));

    for (const auto section : std::views::iota(0u, 4u)) {
        WriteRelativeOffset(writer, writer.tell(), base + section * sizeof(std::uint32_t));
        switch (section) {
            case 0: {
                writer.write(static_cast<std::uint32_t>(table.vocals.notes.size()));
                if (table.vocals.notes.empty()) {
                    break;
                }
                writer.write(table.vocals._04);
                for (const auto& note : table.vocals.notes) {
                    writer.write(note.start);
                    writer.write(note.duration);
                    writer.writeArrayFixed(note.data);
                }
                break;
            }
            case 1: {
                writer.write(static_cast<std::uint32_t>(table.guitar.size()));
                for (const auto& note : table.guitar) {
                    writer.write(note.action);
                    writer.write(note.note.start);
                    writer.write(note.note.duration);
                    writer.writeArrayFixed(note.note.data);
                }
                break;
            }
            case 2:
            case 3: {
                const auto& values = section == 2 ? table.pitchBends : table.vibrato;
                const auto vocalTypeLayout = table.vocals.notes.empty() || table.vocals._04 != -1
                                            || std::max_element(
                                                table.vocals.notes.begin(), table.vocals.notes.end(),
                                                [](const auto& a, const auto& b) { return a.data[0] < b.data[0]; }
                                            )->data[0] >= 8;
                const auto numGroups = vocalTypeLayout ? 11u : 6u;
                if (values.size() != static_cast<size_t>(numGroups)) {
                    return false;
                }
                const auto offsetBase = writer.tell();
                writer.seek(offsetBase + numGroups * sizeof(std::uint32_t));
                for (const auto i : std::views::iota(0u, numGroups)) {
                    if (values[i].values.empty()) {
                        writer.writeAt(0u, offsetBase + i * sizeof(std::uint32_t));
                        continue;
                    }
                    WriteRelativeOffset(writer, writer.tell(), offsetBase + i * sizeof(std::uint32_t));
                    writer.write(static_cast<std::uint32_t>(values[i].values.size()));
                    writer.write(values[i]._04);
                    for (const auto& value : values[i].values) {
                        writer.write(value.samplePosition);
                        writer.write(value.value);
                    }
                }
                break;
            }
        }
    }
    return true;
}

auto MinfWriter::WriteACNHDJSequenceTable(const sound::park::DJMusic& table, common::BinaryWriter& writer) -> bool {
    writer.write(7u);
    const auto base = writer.tell();
    writer.seek(base + 7 * sizeof(std::uint32_t));

    for (const auto section : std::views::iota(0u, 7u)) {
        WriteRelativeOffset(writer, writer.tell(), base + section * sizeof(std::uint32_t));
        switch (section) {
            case 0: {
                writer.write(static_cast<std::uint32_t>(table.vocals.notes.size()));
                if (table.vocals.notes.empty()) {
                    break;
                }
                writer.write(table.vocals._04);
                for (const auto& note : table.vocals.notes) {
                    writer.write(note.start);
                    writer.write(note.duration);
                    writer.writeArrayFixed(note.data);
                }
                break;
            }
            case 1: {
                writer.write(static_cast<std::uint32_t>(table.guitar.size()));
                for (const auto& note : table.guitar) {
                    writer.write(note.action);
                    writer.write(note.note.start);
                    writer.write(note.note.duration);
                    writer.writeArrayFixed(note.note.data);
                }
                break;
            }
            case 2:
            case 3:
            case 4:
            case 5:
            case 6: {
                const auto& notes = section == 2 ? table.rhythm
                                  : section == 3 ? table.bass
                                  : section == 4 ? table.piano
                                  : section == 5 ? table.synth
                                  : table.break_;
                writer.write(static_cast<std::uint32_t>(notes.size()));
                for (const auto& note : notes) {
                    writer.write(note.start);
                    writer.write(note.duration);
                    writer.writeArrayFixed(note.data);
                }
                break;
            }
        }
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
    writer.write(info.getLoopEnd());
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

    if (info.getChordTable()) {
        const auto offset = writer.tell();
        WriteRelativeOffset(writer, offset, base + offsetof(resource::ResMusicInfo, chordOffset));

        const auto& chordTable = *info.getChordTable();
        writer.write(static_cast<std::uint16_t>(chordTable.entries.size()));
        writer.write(chordTable.loopBaseIndex);
        const auto base = writer.tell();
        writer.seek(writer.tell() + chordTable.entries.size() * sizeof(resource::ResChordOffset));
        for (const auto& [i, chord] : chordTable.entries | std::views::enumerate) {
            writer.writeAt(chord.samplePos, base + i * sizeof(resource::ResChordOffset) + offsetof(resource::ResChordOffset, samplePos));
            WriteRelativeOffset(writer, writer.tell(), base + i * sizeof(resource::ResChordOffset) + offsetof(resource::ResChordOffset, offset));

            if (!WriteChord(chord, writer)) {
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

    if (info.getSequenceData()) {
        const auto offset = writer.tell();
        WriteRelativeOffset(writer, offset, base + offsetof(resource::ResMusicInfo, sequenceOffset));

        if (!std::visit(
            [&](auto&& tbl) {
                using T = std::decay_t<decltype(tbl)>;
                if constexpr (std::is_same_v<T, std::unique_ptr<sound::thunder::SequenceTable>>) {
                    return WriteS3SequenceTable(*tbl, writer);
                } else if constexpr (std::is_same_v<T, std::unique_ptr<sound::park::GenericMusic>>) {
                    return WriteACNHSequenceTable(*tbl, writer);
                } else if constexpr (std::is_same_v<T, std::unique_ptr<sound::park::DJMusic>>) {
                    return WriteACNHDJSequenceTable(*tbl, writer);
                } else {
                    static_assert(false, "Unreachable case");
                }
            },
            *info.getSequenceData()
        )) {
            return false;
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