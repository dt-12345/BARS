#include "common/bitflag.hpp"
#include "common/hash.hpp"
#include "encode/amtaWriter.hpp"
#include "encode/minfWriter.hpp"
#include "resource/amta.hpp"
#include "sound/metadata.hpp"

#include <filesystem>
#include <fstream>
#include <ranges>

namespace encode {

static constexpr auto ConvertAttenuationChannel(sound::ChannelPan ch) -> resource::AttenuationChannel {
    switch (ch) {
        case sound::ChannelPan::Left:
            return resource::AttenuationChannel::Left;
        case sound::ChannelPan::Right:
            return resource::AttenuationChannel::Right;
        case sound::ChannelPan::LeftSurround:
            return resource::AttenuationChannel::LeftSurround;
        case sound::ChannelPan::RightSurround:
            return resource::AttenuationChannel::RightSurround;
        case sound::ChannelPan::Center:
            return resource::AttenuationChannel::Center;
        case sound::ChannelPan::LowFrequencyEffect:
            return resource::AttenuationChannel::LowFrequencyEffect;
        default:
            return resource::AttenuationChannel::Center;
    }
}

auto AmtaWriter::WriteRelativeOffset(common::BinaryWriter& writer, std::uint32_t offsetValue, std::uint32_t offsetBase) -> void {
    writer.writeAt(offsetValue - offsetBase, offsetBase);
}

auto AmtaWriter::WriteOptData(const sound::OptionalMetadata& opts, common::BinaryWriter& writer) -> bool {
    auto flags = common::BitFlag<resource::OptFlags>();
    flags.set(resource::OptFlags::MaxAmplitude, opts.maxAmplitude.has_value());
    flags.set(resource::OptFlags::_01, opts._01.has_value());
    flags.set(resource::OptFlags::MaxMomentaryLufs, opts.maxMomentaryLufs.has_value());
    flags.set(resource::OptFlags::IntegratedLufs, opts.integratedLufs.has_value());
    flags.set(resource::OptFlags::TailLength, opts.tailLength.has_value());
    flags.set(resource::OptFlags::_05, opts._05.has_value());
    flags.set(resource::OptFlags::_06, opts._06.has_value());
    flags.set(resource::OptFlags::_07, opts._07.has_value());
    writer.write(static_cast<std::uint32_t>(flags));

    if (opts.maxAmplitude) {
        writer.write(*opts.maxAmplitude);
    }
    if (opts._01) {
        writer.write(*opts._01);
    }
    if (opts.maxMomentaryLufs) {
        writer.write(*opts.maxMomentaryLufs);
    }
    if (opts.integratedLufs) {
        writer.write(*opts.integratedLufs);
    }
    if (opts.tailLength) {
        writer.write(*opts.tailLength);
    }
    if (opts._05) {
        writer.write(static_cast<std::uint16_t>(opts._05->points.size()));
        writer.write(opts._05->_01);
    }
    if (opts._06) {
        writer.write(*opts._06);
    }
    if (opts._07) {
        writer.write(*opts._07);
    }
    if (opts._05) {
        for (const auto& point : opts._05->points) {
            writer.write(point.samplePos);
            writer.write(point._04);
        }
    }

    return true;
}

auto AmtaWriter::WriteMarker(const sound::Marker& marker, common::BinaryWriter& writer, std::map<std::uint32_t, std::string>& strings) -> bool {
    writer.write(marker.id);
    strings.emplace(writer.tell(), marker.name);
    writer.skip(4);
    writer.write(marker.start);
    writer.write(marker.duration);
    return true;
}

auto AmtaWriter::Write(const sound::Metadata& meta, common::BinaryWriter& writer) -> bool {
    writer.setEndian(meta.getEndian());
    const auto base = writer.tell();

    auto stringOffsets = std::map<std::uint32_t, std::string>{};

    writer.writeArrayFixed<char, 4>({ 'A', 'M', 'T', 'A' });
    writer.write(static_cast<std::uint16_t>(0xfeff));
    writer.write(static_cast<std::uint16_t>(0x500));
    writer.skip(0x1c); // file size and various offsets, fill in later
    stringOffsets.emplace(writer.tell(), meta.getName());
    writer.skip(4);
    writer.write(common::CalcCRC32(meta.getName()));
    
    auto flags = common::BitFlag<resource::MetaFlags>();
    flags.set(resource::MetaFlags::IsStreaming, meta.getIsStreaming());
    flags.set(resource::MetaFlags::HasLocalSamples, meta.getHasSoundInArchive());
    flags.set(resource::MetaFlags::IsLoop, meta.getIsLoop());
    flags.set(resource::MetaFlags::IsOpus, meta.getIsOpus());
    flags.set(resource::MetaFlags::IsOpusDecodeFourFramesAtOnce, meta.getIsBatchOpusDecode());
    flags.set(resource::MetaFlags::IsUniformAttenuationPerTrack, meta.getUniformTrackAttenutation());
    writer.write(static_cast<std::uint32_t>(flags));

    writer.write(static_cast<std::uint8_t>(meta.getTrackCount()));
    for (const auto i : std::views::iota(0u, meta.getTrackCount())) {
        const auto& track = meta.getTrack(i);
        writer.write(static_cast<std::uint8_t>(track.getChannelCount()));
        for (const auto j : std::views::iota(0u, track.getChannelCount())) {
            writer.write(track.getInputChannel(j));
            writer.write(ConvertAttenuationChannel(track.getOutputChannel(j)));
        }
    }

    writer.alignUp(4);
    if (meta.getOptData()) {
        const auto offset = static_cast<std::uint32_t>(writer.tell() - base);
        writer.writeAt(offset, base + offsetof(resource::ResAudioMetadata, dataOffset));

        if (!WriteOptData(*meta.getOptData(), writer)) {
            return false;
        }
    }

    writer.alignUp(4);
    if (meta.getMarkerCount()) {
        const auto offset = static_cast<std::uint32_t>(writer.tell() - base);
        writer.writeAt(offset, base + offsetof(resource::ResAudioMetadata, markerOffset));

        writer.write(meta.getMarkerCount());
        for (const auto& marker : meta.getMarkers()) {
            if (!WriteMarker(marker, writer, stringOffsets)) {
                return false;
            }
        }
    }

    writer.alignUp(4);
    if (meta.getMusicInfo()) {
        const auto offset = static_cast<std::uint32_t>(writer.tell() - base);
        writer.writeAt(offset, base + offsetof(resource::ResAudioMetadata, musicInfoOffset));

        if (!MinfWriter::Write(*meta.getMusicInfo(), writer)) {
            return false;
        }
        // minf writer may alter endianness so we need to restore it
        writer.setEndian(meta.getEndian());
    }

    writer.alignUp(4);
    if (meta.getTagCount()) {
        const auto offset = static_cast<std::uint32_t>(writer.tell() - base);
        writer.writeAt(offset, base + offsetof(resource::ResAudioMetadata, tagOffset));

        writer.write(meta.getTagCount());
        for (const auto& tag : meta.getTags()) {
            stringOffsets.emplace(writer.tell(), tag);
            writer.skip(4);
        }
    }

    writer.alignUp(4);
    if (meta.getAttributeCount()) {
        const auto offset = static_cast<std::uint32_t>(writer.tell() - base);
        writer.writeAt(offset, base + offsetof(resource::ResAudioMetadata, attrOffset));

        writer.write(meta.getAttributeCount());
        for (const auto& [attr, value] : meta.getAttributes()) {
            stringOffsets.emplace(writer.tell(), attr);
            writer.skip(4);
            writer.write(value);
        }
    }

    writer.alignUp(4);
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

    writer.alignUp(4);

    const auto size = static_cast<std::uint32_t>(writer.tell() - base);
    writer.writeAt(size, base + offsetof(resource::ResAudioMetadata, size));
    return true;
}

auto AmtaWriter::Write(const sound::Metadata& meta, std::vector<std::uint8_t>& buffer) -> bool {
    auto writer = common::BinaryWriter();
    if (!Write(meta, writer)) {
        return false;
    }
    buffer.swap(writer.buffer());
    return true;
}

auto AmtaWriter::Write(const sound::Metadata& meta, std::string_view path) -> bool {
    auto buffer = std::vector<std::uint8_t>{};
    if (!Write(meta, buffer)) {
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