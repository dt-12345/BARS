#include "decode/minfReader.hpp"
#include "common/reader.hpp"
#include "decode/decodeResult.hpp"
#include "resource/minf.hpp"
#include "sound/music.hpp"

#include <ranges>

namespace decode {

static inline auto ReadRelativeOffset(common::BinaryReader& reader) -> std::uint32_t {
    const auto base = reader.tell();
    return static_cast<std::uint32_t>(base) + reader.read<std::uint32_t>();
}

auto MinfReader::ReadTempoMeter(common::BinaryReader& reader, sound::TempoMeter& tempoMeter) -> Result {
    if (!reader.checkSize((sizeof(resource::ResTempoMeter)))) {
        return BufferTooSmall;
    }

    tempoMeter.samplePos = reader.read<std::uint32_t>();
    tempoMeter.tempo = reader.read<float>();
    tempoMeter.timeSignature.upper = reader.read<std::uint16_t>();
    tempoMeter.timeSignature.lower = reader.read<std::uint16_t>();
    return OK;
}

auto MinfReader::ReadChord(common::BinaryReader& reader, sound::Chord& chord) -> Result {
    if (!reader.checkSize<std::uint32_t>()) {
        return BufferTooSmall;
    }

    const auto count = reader.read<std::uint32_t>();
    if (!reader.checkSize(common::AlignUp(sizeof(sound::Note) * count, 4ull))) {
        return BufferTooSmall;
    }

    auto notes = reader.readArray<sound::Note>(count);
    chord.notes.swap(notes);
    reader.alignUp(4);
    
    return OK;
}

auto MinfReader::ReadBeat(common::BinaryReader& reader, sound::Beat& beat) -> Result {
    if (!reader.checkSize((sizeof(resource::ResBeat)))) {
        return BufferTooSmall;
    }

    beat.samplePos = reader.read<std::uint32_t>();
    beat.beatNum = reader.read<std::uint32_t>();
    return OK;
}

auto MinfReader::ReadPointMarker(common::BinaryReader& reader, sound::PointMarker& marker) -> Result {
    if (!reader.checkSize<resource::ResMarkerOffset>()) {
        return BufferTooSmall;
    }

    reader.skip(4); // hash
    const auto offset = ReadRelativeOffset(reader);
    const auto returnOffset = reader.tell();
    reader.seek(offset);

    if (!reader.checkSize<resource::ResPointMarker>()) {
        return BufferTooSmall;
    }

    const auto nameOffset = ReadRelativeOffset(reader);

    const auto count = reader.read<std::uint16_t>();
    marker.loopBaseIndex = reader.read<std::int16_t>();

    if (!reader.checkSize(count * sizeof(std::uint32_t))) {
        return BufferTooSmall;
    }

    auto samplePositions = reader.readArray<std::uint32_t>(count);
    marker.samplePositions.swap(samplePositions);
    const auto name = reader.readString(nameOffset);
    if (!name.empty()) {
        marker.name = name;
    }
    reader.seek(returnOffset);
    return OK;
}

auto MinfReader::ReadRangeMarker(common::BinaryReader& reader, sound::RangeMarker& marker) -> Result {
    if (!reader.checkSize<resource::ResMarkerOffset>()) {
        return BufferTooSmall;
    }

    reader.skip(4); // hash
    const auto offset = ReadRelativeOffset(reader);
    const auto returnOffset = reader.tell();
    reader.seek(offset);

    if (!reader.checkSize<resource::ResRangedMarkerRange>()) {
        return BufferTooSmall;
    }

    const auto nameOffset = ReadRelativeOffset(reader);
    const auto endOffset = ReadRelativeOffset(reader);

    if (!reader.checkSize<resource::ResTable>()) {
        return BufferTooSmall;
    }
    const auto startCount = reader.read<std::uint16_t>();
    marker.starts.loopBaseIndex = reader.read<std::int16_t>();
    marker.starts.entries.resize(startCount);
    for (const auto i : std::views::iota(0u, startCount)) {
        marker.starts.entries[i].samplePos = reader.read<std::uint32_t>();
        marker.starts.entries[i]._04 = reader.read<std::uint32_t>();
    }

    reader.seek(endOffset);
    if (!reader.checkSize<resource::ResTable>()) {
        return BufferTooSmall;
    }
    const auto endCount = reader.read<std::uint16_t>();
    marker.ends.loopBaseIndex = reader.read<std::int16_t>();
    marker.ends.entries.resize(endCount);
    for (const auto i : std::views::iota(0u, endCount)) {
        marker.ends.entries[i].samplePos = reader.read<std::uint32_t>();
        marker.ends.entries[i]._04 = reader.read<std::uint32_t>();
    }

    const auto name = reader.readString(nameOffset);
    if (!name.empty()) {
        marker.name = name;
    }
    reader.seek(returnOffset);
    return OK;
}


auto MinfReader::ReadTempoTable(common::BinaryReader& reader, sound::Table<sound::TempoMeter>& table) -> Result {
    if (!reader.checkSize<resource::ResTable>()) {
        return BufferTooSmall;
    }

    const auto count = reader.read<std::uint16_t>();
    table.loopBaseIndex = reader.read<std::int16_t>();
    table.entries.resize(count);

    if (!reader.checkSize(count * sizeof(resource::ResTempoMeter))) {
        return BufferTooSmall;
    }

    for (const auto i : std::views::iota(0u, count)) {
        if (const auto res = ReadTempoMeter(reader, table.entries[i]); res != OK) {
            return res;
        }
    }

    return OK;
}

auto MinfReader::ReadChordTable(common::BinaryReader& reader, sound::Table<sound::Chord>& table) -> Result {
    if (!reader.checkSize<resource::ResTable>()) {
        return BufferTooSmall;
    }

    const auto count = reader.read<std::uint16_t>();
    table.loopBaseIndex = reader.read<std::int16_t>();
    table.entries.resize(count);

    if (!reader.checkSize(count * sizeof(resource::ResChordOffset))) {
        return BufferTooSmall;
    }

    for (const auto i : std::views::iota(0u, count)) {
        auto& chord = table.entries[i];
        chord.samplePos = reader.read<std::uint32_t>();
        const auto base = reader.tell();
        const auto offset = reader.read<std::uint32_t>();
        reader.seek(base + offset);
        if (const auto res = ReadChord(reader, chord); res != OK) {
            return res;
        }
        reader.seek(base + sizeof(std::uint32_t));
    }

    return OK;
}

auto MinfReader::ReadBeatTable(common::BinaryReader& reader, sound::Table<sound::Beat>& table) -> Result {
    if (!reader.checkSize<resource::ResTable>()) {
        return BufferTooSmall;
    }

    const auto count = reader.read<std::uint16_t>();
    table.loopBaseIndex = reader.read<std::int16_t>();
    table.entries.resize(count);

    if (!reader.checkSize(count * sizeof(resource::ResBeat))) {
        return BufferTooSmall;
    }

    for (const auto i : std::views::iota(0u, count)) {
        if (const auto res = ReadBeat(reader, table.entries[i]); res != OK) {
            return res;
        }
    }

    return OK;
}

auto MinfReader::ReadMeasureTable(common::BinaryReader& reader, sound::Table<std::uint32_t>& table) -> Result {
    if (!reader.checkSize<resource::ResTable>()) {
        return BufferTooSmall;
    }

    const auto count = reader.read<std::uint16_t>();
    table.loopBaseIndex = reader.read<std::int16_t>();
    if (!reader.checkSize(count * sizeof(std::uint32_t))) {
        return BufferTooSmall;
    }

    auto samplePositions = reader.readArray<std::uint32_t>(count);
    table.entries.swap(samplePositions);

    return OK;
}

auto MinfReader::ReadPointMarkerTable(common::BinaryReader& reader, std::vector<sound::PointMarker>& table) -> Result {
    if (!reader.checkSize<resource::ResTable>()) {
        return BufferTooSmall;
    }

    const auto count = reader.read<std::uint32_t>();

    for (const auto _ : std::views::iota(0u, count)) {
        auto& marker = table.emplace_back();
        if (const auto res = ReadPointMarker(reader, marker); res != OK) {
            return res;
        }
    }

    return OK;
}

auto MinfReader::ReadRangeMarkerTable(common::BinaryReader& reader, std::vector<sound::RangeMarker>& table) -> Result {
    if (!reader.checkSize<resource::ResTable>()) {
        return BufferTooSmall;
    }

    const auto count = reader.read<std::uint32_t>();

    for (const auto _ : std::views::iota(0u, count)) {
        auto& marker = table.emplace_back();
        if (const auto res = ReadRangeMarker(reader, marker); res != OK) {
            return res;
        }
    }

    return OK;
}

auto MinfReader::ReadS3SequenceTable(common::BinaryReader& reader, sound::thunder::SequenceTable& table) -> Result {
    const auto base = reader.tell();
    const auto offset = reader.read<std::uint32_t>();

    if (offset == 0) {
        return OK;
    }

    reader.seek(base + offset);
    if (!reader.checkSize<std::uint32_t>()) {
        return BufferTooSmall;
    }
    
    const auto count = reader.read<std::uint32_t>();
    for (const auto _ : std::views::iota(0u, count)) {
        auto& track = table.emplace_back();
        if (!reader.checkSize<std::uint32_t>()) {
            return BufferTooSmall;
        }
        const auto nameLen = reader.read<std::uint32_t>();
        if (!reader.checkSize(sizeof(char) * nameLen)) {
            return BufferTooSmall;
        }
        const auto name = reader.readArray<char>(nameLen);
        track.name.assign(name.begin(), name.end());
        if (!reader.checkSize<std::uint32_t>()) {
            return BufferTooSmall;
        }
        const auto noteCount = reader.read<std::uint32_t>();
        if (!reader.checkSize(sizeof(sound::thunder::ResNote) * noteCount)) {
            return BufferTooSmall;
        }
        for (const auto _ : std::views::iota(0u, noteCount)) {
            auto& note = track.notes.emplace_back();
            note.start = reader.read<std::uint32_t>();
            note.end = reader.read<std::uint32_t>();
            note.pitch = reader.read<std::uint16_t>();
            note.velocity = reader.read<std::uint16_t>();
            note._0c = reader.read<std::uint16_t>();
        }
        const auto count2 = reader.read<std::uint32_t>();
        if (!reader.checkSize(sizeof(sound::thunder::ResSequence2) * count2)) {
            return BufferTooSmall;
        }
        for (const auto _ : std::views::iota(0u, count2)) {
            auto& entry = track._02.emplace_back();
            entry._00 = reader.read<std::uint32_t>();
            entry._04 = reader.read<std::uint32_t>();
        }
        const auto count3 = reader.read<std::uint32_t>();
        if (!reader.checkSize(sizeof(sound::thunder::ResSequence3) * count3)) {
            return BufferTooSmall;
        }
        for (const auto _ : std::views::iota(0u, count3)) {
            auto& entry = track._03.emplace_back();
            entry.samplePosition = reader.read<std::uint32_t>();
            entry._04 = reader.read<std::uint16_t>();
        }
    }

    reader.seek(base + sizeof(std::uint32_t));
    return OK;
}

auto MinfReader::ReadACNHSequenceTable(common::BinaryReader& reader, sound::park::GenericMusic& table) -> Result {
    auto isByVoiceType = false;
    for (const auto section : std::views::iota(0u, 4u)) {
        const auto base = reader.tell();
        const auto offset = reader.read<std::uint32_t>();
        if (offset == 0) {
            continue;
        }
        reader.seek(base + offset);
        switch (section) {
            case 0: {
                if (!reader.checkSize<std::uint32_t>()) {
                    return BufferTooSmall;
                }
                const auto count = reader.read<std::uint32_t>();
                if (count == 0) {
                    isByVoiceType = true;
                    break;
                }
                if (!reader.checkSize(sizeof(std::int32_t) + sizeof(sound::park::ResNote) * count)) {
                    return BufferTooSmall;
                }
                table.vocals._04 = reader.read<std::int32_t>();
                auto maxVoiceType = std::uint8_t(0);
                for (const auto _ : std::views::iota(0u, count)) {
                    auto& note = table.vocals.notes.emplace_back();
                    note.start = reader.read<std::uint32_t>();
                    note.duration = reader.read<std::uint32_t>();
                    note.data = reader.readArrayFixed<std::uint8_t, 4>();
                    maxVoiceType = std::max(maxVoiceType, note.data[0]);
                }
                isByVoiceType = table.vocals._04 != -1 || maxVoiceType >= 8;
                break;
            }
            case 1: {
                if (!reader.checkSize<std::uint32_t>()) {
                    return BufferTooSmall;
                }
                const auto count = reader.read<std::uint32_t>();
                if (!reader.checkSize(sizeof(sound::park::ResGuitarNote) * count)) {
                    return BufferTooSmall;
                }
                for (const auto _ : std::views::iota(0u, count)) {
                    auto& note = table.guitar.emplace_back();
                    note.action = reader.read<sound::park::GuitarAction>();
                    note.note.start = reader.read<std::uint32_t>();
                    note.note.duration = reader.read<std::uint32_t>();
                    note.note.data = reader.readArrayFixed<std::uint8_t, 4>();
                }
                break;
            }
            case 2:
            case 3: {
                const auto groups = isByVoiceType ? 11u : 6u;
                if (!reader.checkSize(sizeof(std::uint32_t) * groups)) {
                    return BufferTooSmall;
                }

                for (const auto _ : std::views::iota(0u, groups)) {
                    const auto offset = reader.read<std::uint32_t>();
                    if (offset == 0) {
                        if (section == 2) {
                            table.pitchBends.emplace_back();
                        } else {
                            table.vibrato.emplace_back();
                        }
                        continue;
                    }

                    const auto base = reader.tell();
                    reader.seek(base + offset - sizeof(std::uint32_t));
                    if (!reader.checkSize(sizeof(std::uint32_t) + sizeof(std::int32_t))) {
                        return BufferTooSmall;
                    }
                    auto& values = section == 2 ? table.pitchBends.emplace_back() : table.vibrato.emplace_back();
                    const auto count = reader.read<std::uint32_t>();
                    values._04 = reader.read<std::int32_t>();
                    if (!reader.checkSize(sizeof(sound::park::ResPitchValue) * count)) {
                        return BufferTooSmall;
                    }
                    for (const auto _ : std::views::iota(0u, count)) {
                        auto& value = values.values.emplace_back();
                        value.samplePosition = reader.read<std::uint32_t>();
                        value.value = reader.read<std::int32_t>();
                    }
                    reader.seek(base);
                }
                break;
            }
        }
        reader.seek(base + sizeof(std::uint32_t));
    }

    return OK;
}

auto MinfReader::ReadACNHDJSequenceTable(common::BinaryReader& reader, sound::park::DJMusic& table) -> Result {
    for (const auto section : std::views::iota(0u, 7u)) {
        const auto base = reader.tell();
        const auto offset = reader.read<std::uint32_t>();
        if (offset == 0) {
            continue;
        }
        reader.seek(base + offset);
        switch (section) {
            case 0: {
                if (!reader.checkSize<std::uint32_t>()) {
                    return BufferTooSmall;
                }
                const auto count = reader.read<std::uint32_t>();
                if (count == 0) {
                    break;
                }
                if (!reader.checkSize(sizeof(std::int32_t) + sizeof(sound::park::ResNote) * count)) {
                    return BufferTooSmall;
                }
                table.vocals._04 = reader.read<std::int32_t>();
                for (const auto _ : std::views::iota(0u, count)) {
                    auto& note = table.vocals.notes.emplace_back();
                    note.start = reader.read<std::uint32_t>();
                    note.duration = reader.read<std::uint32_t>();
                    note.data = reader.readArrayFixed<std::uint8_t, 4>();
                }
                break;
            }
            case 1: {
                if (!reader.checkSize<std::uint32_t>()) {
                    return BufferTooSmall;
                }
                const auto count = reader.read<std::uint32_t>();
                if (!reader.checkSize(sizeof(sound::park::ResGuitarNote) * count)) {
                    return BufferTooSmall;
                }
                for (const auto _ : std::views::iota(0u, count)) {
                    auto& note = table.guitar.emplace_back();
                    note.action = reader.read<sound::park::GuitarAction>();
                    note.note.start = reader.read<std::uint32_t>();
                    note.note.duration = reader.read<std::uint32_t>();
                    note.note.data = reader.readArrayFixed<std::uint8_t, 4>();
                }
                break;
            }
            case 2:
            case 3:
            case 4:
            case 5:
            case 6: {
                if (!reader.checkSize<std::uint32_t>()) {
                    return BufferTooSmall;
                }
                const auto count = reader.read<std::uint32_t>();
                if (!reader.checkSize(sizeof(sound::park::ResNote) * count)) {
                    return BufferTooSmall;
                }
                auto& notes = section == 2 ? table.rhythm
                            : section == 3 ? table.bass
                            : section == 4 ? table.piano
                            : section == 5 ? table.synth
                            : table.break_;
                for (const auto _ : std::views::iota(0u, count)) {
                    auto& note = notes.emplace_back();
                    note.start = reader.read<std::uint32_t>();
                    note.duration = reader.read<std::uint32_t>();
                    note.data = reader.readArrayFixed<std::uint8_t, 4>();
                }
                break;
            }
        }
        reader.seek(base + sizeof(std::uint32_t));
    }
    return OK;
}

auto MinfReader::Read(common::BinaryReader& reader, sound::MusicInfo& info) -> Result {
    if (!reader.checkSize<resource::ResMusicInfo>()) {
        return BufferTooSmall;
    }

    const auto baseOffset = reader.tell();

    const auto magic = reader.readArrayFixed<char, 4>();
    if (magic[0] != 'M' || magic[1] != 'I' || magic[2] != 'N' || magic[3] != 'F') {
        return InvalidMinfMagic;
    }

    const auto bom = reader.read<std::uint16_t>(std::endian::big);
    reader.setEndian(bom == 0xfffe ? std::endian::little : std::endian::big);
    
    const auto version = reader.read<std::uint16_t>();
    if (version != 0x101) {
        return InvalidMinfVersion;
    }

    const auto fileSize = reader.read<std::uint32_t>();

    const auto enNameOffset = ReadRelativeOffset(reader);
    const auto jpNameOffset = ReadRelativeOffset(reader);

    info.setEndian(bom == 0xfffe ? std::endian::little : std::endian::big);
    info.setSampleRate(reader.read<std::uint32_t>());
    info.setLoopStart(reader.read<std::uint32_t>());
    info.setLoopEnd(reader.read<std::uint32_t>());

    if (const auto res = ReadTempoMeter(reader, info.getDefaultTempoMeter()); res != OK) {
        return res;
    }

    const auto tempoOffset = reader.read<std::uint32_t>();
    const auto chordOffset = reader.read<std::uint32_t>();
    [[maybe_unused]] const auto _34 = reader.read<std::uint32_t>();
    const auto beatOffset = reader.read<std::uint32_t>();
    const auto measureOffset = reader.read<std::uint32_t>();
    const auto pointMarkerOffset = reader.read<std::uint32_t>();
    const auto rangedMarkerOffset = reader.read<std::uint32_t>();
    const auto sequenceOffset = reader.read<std::uint32_t>();

    const auto enName = reader.readString(enNameOffset);
    if (enNameOffset + enName.size() + 1 > baseOffset + fileSize) {
        return BufferTooSmall;
    }
    if (!enName.empty()) {
        info.setTrackName(enName);
    }

    const auto jpName = reader.readString(jpNameOffset);
    if (jpNameOffset + jpName.size() + 1 > baseOffset + fileSize) {
        return BufferTooSmall;
    }
    if (!jpName.empty()) {
        info.setJapaneseName(jpName);
    }

    if (tempoOffset) {
        reader.seek(baseOffset + offsetof(resource::ResMusicInfo, tempoOffset) + tempoOffset);
        info.initTempoMeterTable();
        if (const auto res = ReadTempoTable(reader, *info.getTempoMeterTable()); res != OK) {
            return res;
        }
    }

    if (chordOffset) {
        reader.seek(baseOffset + offsetof(resource::ResMusicInfo, chordOffset) + chordOffset);
        info.initChordTable();
        if (const auto res = ReadChordTable(reader, *info.getChordTable()); res != OK) {
            return res;
        }
    }

    if (beatOffset) {
        reader.seek(baseOffset + offsetof(resource::ResMusicInfo, beatOffset) + beatOffset);
        info.initBeatTable();
        if (const auto res = ReadBeatTable(reader, *info.getBeatTable()); res != OK) {
            return res;
        }
    }

    if (measureOffset) {
        reader.seek(baseOffset + offsetof(resource::ResMusicInfo, measureOffset) + measureOffset);
        info.initMeasureTable();
        if (const auto res = ReadMeasureTable(reader, *info.getMeasureTable()); res != OK) {
            return res;
        }
    }

    if (pointMarkerOffset) {
        reader.seek(baseOffset + offsetof(resource::ResMusicInfo, pointMarkerOffset) + pointMarkerOffset);
        info.initPointMarkerTable();
        if (const auto res = ReadPointMarkerTable(reader, *info.getPointMarkerTable()); res != OK) {
            return res;
        }
    }

    if (rangedMarkerOffset) {
        reader.seek(baseOffset + offsetof(resource::ResMusicInfo, rangedMarkerOffset) + rangedMarkerOffset);
        info.initRangeMarkerTable();
        if (const auto res = ReadRangeMarkerTable(reader, *info.getRangeMarkerTable()); res != OK) {
            return res;
        }
    }

    if (sequenceOffset) {
        reader.seek(baseOffset + offsetof(resource::ResMusicInfo, sequenceOffset) + sequenceOffset);
        const auto sectionCount = reader.read<std::uint32_t>();

        if (!reader.checkSize(sectionCount * sizeof(std::uint32_t))) {
            return BufferTooSmall;
        }

        // this probably isn't real, but it's the only way we have of differentiating the variations
        // this section seems to be entirely game-specific
        // also this setup sucks lmao
        switch (sectionCount) {
            case 1: {
                info.initS3SequenceData();
                if (const auto res = ReadS3SequenceTable(reader, *std::get<0>(*info.getSequenceData())); res != OK) {
                    return res;
                }
                break;
            }
            case 4: {
                info.initACNHSequenceData();
                if (const auto res = ReadACNHSequenceTable(reader, *std::get<1>(*info.getSequenceData())); res != OK) {
                    return res;
                }
                break;
            }
            case 7: {
                info.initACNHDJSequenceData();
                if (const auto res = ReadACNHDJSequenceTable(reader, *std::get<2>(*info.getSequenceData())); res != OK) {
                    return res;
                }
                break;
            }
        }
    }

    return OK;
}

} // namespace decode