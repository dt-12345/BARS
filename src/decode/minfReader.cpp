#include "decode/minfReader.hpp"
#include "resource/minf.hpp"

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

auto MinfReader::ReadBeat(common::BinaryReader& reader, sound::Beat& beat) -> Result {
    if (!reader.checkSize((sizeof(resource::ResBeat)))) {
        return BufferTooSmall;
    }

    beat.samplePos = reader.read<std::uint32_t>();
    beat.beatNum = reader.read<std::uint32_t>();
    return OK;
}

auto MinfReader::ReadPointMarker(common::BinaryReader& reader, sound::PointMarker& marker) -> Result {
    if (!reader.checkSize(sizeof(resource::ResMarkerOffset))) {
        return BufferTooSmall;
    }

    reader.skip(4); // hash
    const auto offset = ReadRelativeOffset(reader);
    const auto returnOffset = reader.tell();
    reader.seek(offset);

    if (!reader.checkSize(sizeof(resource::ResPointMarker))) {
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
    if (!reader.checkSize(sizeof(resource::ResMarkerOffset))) {
        return BufferTooSmall;
    }

    reader.skip(4); // hash
    const auto offset = ReadRelativeOffset(reader);
    const auto returnOffset = reader.tell();
    reader.seek(offset);

    if (!reader.checkSize(sizeof(resource::ResRangedMarkerRange))) {
        return BufferTooSmall;
    }

    const auto nameOffset = ReadRelativeOffset(reader);
    const auto endOffset = ReadRelativeOffset(reader);

    if (!reader.checkSize(sizeof(resource::ResTable))) {
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
    if (!reader.checkSize(sizeof(resource::ResTable))) {
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
    if (!reader.checkSize(sizeof(resource::ResTable))) {
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

auto MinfReader::ReadBeatTable(common::BinaryReader& reader, sound::Table<sound::Beat>& table) -> Result {
    if (!reader.checkSize(sizeof(resource::ResTable))) {
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
    if (!reader.checkSize(sizeof(resource::ResTable))) {
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
    if (!reader.checkSize(sizeof(resource::ResTable))) {
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
    if (!reader.checkSize(sizeof(resource::ResTable))) {
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


auto MinfReader::Read(common::BinaryReader& reader, sound::MusicInfo& info) -> Result {
    if (!reader.checkSize(sizeof(resource::ResMusicInfo))) {
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
    info.setSampleCount(reader.read<std::uint32_t>());

    if (const auto res = ReadTempoMeter(reader, info.getDefaultTempoMeter()); res != OK) {
        return res;
    }

    const auto tempoOffset = reader.read<std::uint32_t>();
    [[maybe_unused]] const auto _30 = reader.read<std::uint32_t>();
    [[maybe_unused]] const auto _34 = reader.read<std::uint32_t>();
    const auto beatOffset = reader.read<std::uint32_t>();
    const auto measureOffset = reader.read<std::uint32_t>();
    const auto pointMarkerOffset = reader.read<std::uint32_t>();
    const auto rangedMarkerOffset = reader.read<std::uint32_t>();
    [[maybe_unused]] const auto _48 = reader.read<std::uint32_t>();

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

    return OK;
}

} // namespace decode