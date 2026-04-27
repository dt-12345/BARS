#include "common/bitflag.hpp"
#include "decode/amtaReader.hpp"
#include "decode/minfReader.hpp"
#include "resource/amta.hpp"
#include "sound/types.hpp"

#include <ranges>

namespace decode {

static constexpr auto ConvertAttenuationChannel(resource::AttenuationChannel ch) -> sound::ChannelPan {
    switch (ch) {
        case resource::AttenuationChannel::Left:
            return sound::ChannelPan::Left;
        case resource::AttenuationChannel::Right:
            return sound::ChannelPan::Right;
        case resource::AttenuationChannel::LeftSurround:
            return sound::ChannelPan::LeftSurround;
        case resource::AttenuationChannel::RightSurround:
            return sound::ChannelPan::RightSurround;
        case resource::AttenuationChannel::Center:
            return sound::ChannelPan::Center;
        case resource::AttenuationChannel::LowFrequencyEffect:
            return sound::ChannelPan::LowFrequencyEffect;
        default:
            return sound::ChannelPan::Center;
    }
}

auto AmtaReader::ReadTrack(common::BinaryReader& reader, sound::Track& track) -> Result {
    if (!reader.checkSize(sizeof(std::uint8_t))) {
        return BufferTooSmall;
    }

    const auto count = reader.read<std::uint8_t>();
    if (count > sound::cMaxChannelsPerTrack) {
        return TooManyChannels;
    }

    if (!reader.checkSize(count * sizeof(resource::ResTrackChannel))) {
        return BufferTooSmall;
    }

    for (const auto _ : std::views::iota(0u, count)) {
        const auto inputChannelIndex = reader.read<std::uint8_t>();
        const auto outputChannelPan = ConvertAttenuationChannel(reader.read<resource::AttenuationChannel>());
        if (!track.addChannel(inputChannelIndex, outputChannelPan)) {
            return TooManyChannels;
        }
    }

    return OK;
}

auto AmtaReader::ReadOptData(common::BinaryReader& reader, sound::OptionalMetadata& opt) -> Result {
    if (!reader.checkSize(sizeof(resource::OptFlags))) {
        return BufferTooSmall;
    }

    const auto flags = common::BitFlag<resource::OptFlags>(reader.read<std::uint32_t>());
    if (!reader.checkSize(flags.getNumOn() * sizeof(std::uint32_t))) {
        return BufferTooSmall;
    }

    if (flags.isOn(resource::OptFlags::MaxAmplitude)) {
        opt.maxAmplitude = reader.read<float>();
    }
    if (flags.isOn(resource::OptFlags::_01)) {
        opt._01 = reader.read<float>();
    }
    if (flags.isOn(resource::OptFlags::MaxMomentaryLufs)) {
        opt.maxMomentaryLufs = reader.read<float>();
    }
    if (flags.isOn(resource::OptFlags::IntegratedLufs)) {
        opt.integratedLufs = reader.read<float>();
    }
    if (flags.isOn(resource::OptFlags::TailLength)) {
        opt.tailLength = reader.read<std::uint32_t>();
    }
    if (flags.isOn(resource::OptFlags::_05)) {
        auto& info = opt._05.emplace();
        const auto count = reader.read<std::uint16_t>();
        info.points.resize(count);
        info._01 = reader.read<std::uint16_t>();
    }
    if (flags.isOn(resource::OptFlags::_06)) {
        opt._06 = reader.read<float>();
    }
    if (flags.isOn(resource::OptFlags::_07)) {
        opt._07 = reader.read<std::uint32_t>();
    }

    if (flags.isOn(resource::OptFlags::_05)) {
        if (!reader.checkSize(opt._05->points.size() * sizeof(resource::ResPoint))) {
            return BufferTooSmall;
        }

        for (auto& point : opt._05->points) {
            point.samplePos = reader.read<std::uint32_t>();
            point._04 = reader.read<float>();
        }
    }

    return OK;
}

auto AmtaReader::Read(common::BinaryReader& reader, sound::Metadata& meta) -> Result {
    if (!reader.checkSize(sizeof(resource::ResAudioMetadata))) {
        return BufferTooSmall;
    }

    const auto baseOffset = reader.tell();
    
    const auto magic = reader.readArrayFixed<char, 4>();
    if (magic[0] != 'A' || magic[1] != 'M' || magic[2] != 'T' || magic[3] != 'A') {
        return InvalidAmtaMagic;
    }

    const auto bom = reader.read<std::uint16_t>(std::endian::big);
    reader.setEndian(bom == 0xfffe ? std::endian::little : std::endian::big);

    const auto version = reader.read<std::uint16_t>();
    if (version != 0x500) {
        return InvalidAmtaVersion;
    }

    const auto fileSize = reader.read<std::uint32_t>();
    [[maybe_unused]] const auto offset0xc = reader.read<std::uint32_t>();
    const auto dataOffset = reader.read<std::uint32_t>();
    const auto markerOffset = reader.read<std::uint32_t>();
    const auto musicInfoOffset = reader.read<std::uint32_t>();
    const auto tagOffset = reader.read<std::uint32_t>();
    [[maybe_unused]] const auto offset0x20 = reader.read<std::uint32_t>();
    const auto nameOffsetOffset = reader.tell();
    const auto nameOffset = nameOffsetOffset + reader.read<std::uint32_t>();
    reader.skip(4); // hash

    const auto flags = common::BitFlag<resource::MetaFlags>(reader.read<std::uint32_t>());
    const auto trackCount = reader.read<std::uint8_t>();

    if (trackCount > sound::cMaxTracks) {
        return TooManyTracks;
    }

    for (const auto i : std::views::iota(0u, trackCount)) {
        meta.addTrack();
        if (const auto res = ReadTrack(reader, meta.getTrack(i)); res != OK) {
            return res;
        }
    }

    meta.setEndian(bom == 0xfffe ? std::endian::little : std::endian::big);

    // TODO: flags 3 + 5
    meta.setIsStreaming(flags.isOn(resource::MetaFlags::IsStreaming));
    meta.setHasSoundInArchive(flags.isOn(resource::MetaFlags::HasLocalSamples));
    meta.setIsLoop(flags.isOn(resource::MetaFlags::IsLoop));
    meta.setIsOpus(flags.isOn(resource::MetaFlags::IsOpus));
    meta.setIsBatchOpusDecode(flags.isOn(resource::MetaFlags::IsOpusDecodeFourFramesAtOnce));
    meta.setUniformTrackAttenutation(flags.isOn(resource::MetaFlags::IsUniformAttenuationPerTrack));

    const auto name = reader.readString(nameOffset);
    if (nameOffset + name.size() + 1 > baseOffset + fileSize) {
        return BufferTooSmall;
    }
    meta.setName(name);

    if (dataOffset) {
        reader.seek(baseOffset + dataOffset);
        meta.initOptData();
        if (const auto res = ReadOptData(reader, *meta.getOptData()); res != OK) {
            return res;
        }
    }

    if (markerOffset) {
        reader.seek(baseOffset + markerOffset);
        if (!reader.checkSize(sizeof(std::uint32_t))) {
            return BufferTooSmall;
        }
        const auto count = reader.read<std::uint32_t>();
        if (!reader.checkSize(count * sizeof(resource::ResMarker))) {
            return BufferTooSmall;
        }
        for (const auto _ : std::views::iota(0u, count)) {
            auto& marker = meta.addMarker();
            marker.id = reader.read<std::uint32_t>();
            const auto startOffset = reader.tell();
            const auto nameOffset = startOffset + reader.read<std::uint32_t>();
            marker.start = reader.read<std::uint32_t>();
            marker.duration = reader.read<std::uint32_t>();
            const auto name = reader.readString(nameOffset);
            if (!name.empty()) {
                marker.name = name;
            }
        }
    }

    if (musicInfoOffset) {
        reader.seek(baseOffset + musicInfoOffset);
        meta.initMusicInfo();
        if (const auto res = MinfReader::Read(reader, *meta.getMusicInfo()); res != OK) {
            return res;
        }
    }

    if (tagOffset) {
        reader.seek(baseOffset + tagOffset);
        if (!reader.checkSize(sizeof(std::uint32_t))) {
            return BufferTooSmall;
        }
        const auto count = reader.read<std::uint32_t>();
        if (!reader.checkSize(count * sizeof(std::uint32_t))) {
            return BufferTooSmall;
        }
        for (const auto _ : std::views::iota(0u, count)) {
            const auto startOffset = reader.tell();
            const auto offset = startOffset + reader.read<std::uint32_t>();
            meta.addTag(reader.readString(offset));
        }
    }

    return OK;
}
    
} // namespace decode