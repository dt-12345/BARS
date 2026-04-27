#include "common/align.hpp"
#include "common/hash.hpp"
#include "common/writer.hpp"
#include "encode/bwavWriter.hpp"
#include "decode/adpcmDecoder.hpp"
#include "decode/decoder.hpp"
#include "resource/adpcm.hpp"
#include "resource/bwav.hpp"
#include "sound/sound.hpp"

#include <filesystem>
#include <fstream>
#include <numeric>
#include <ranges>

namespace encode {

static constexpr auto ConvertSampleFormat(sound::Format fmt) -> resource::SampleFormat {
    switch (fmt) {
        case sound::Format::PcmInt16:
            return resource::SampleFormat::PcmInt16;
        case sound::Format::DspAdpcm:
            return resource::SampleFormat::Adpcm;
        case sound::Format::Opus:
            return resource::SampleFormat::Opus;
        default:
            return resource::SampleFormat::PcmInt16;
    }
}

static constexpr auto ConvertOutputChannel(sound::ChannelPan ch) -> resource::OutputChannel {
    switch (ch) {
        case sound::ChannelPan::Left:
            return resource::OutputChannel::Left;
        case sound::ChannelPan::Right:
            return resource::OutputChannel::Right;
        case sound::ChannelPan::Center:
            return resource::OutputChannel::Center;
        case sound::ChannelPan::LowFrequencyEffect:
            return resource::OutputChannel::LowFrequencyEffect;
        case sound::ChannelPan::LeftSurround:
            return resource::OutputChannel::LeftSurround;
        case sound::ChannelPan::RightSurround:
            return resource::OutputChannel::RightSurround;
        default:
            return resource::OutputChannel::Center;
    }
}

static constexpr auto ConvertAssetType(sound::AssetType type) -> resource::AssetType {
    switch (type) {
        case sound::AssetType::Normal:
            return resource::AssetType::Normal;
        case sound::AssetType::Prefetch:
            return resource::AssetType::Prefetch;
        default:
            return resource::AssetType::Invalid;
    }
}

static auto CalcDataHash(const sound::Sound& sound) -> std::uint32_t {
    auto ctx = common::CRC32Context{};
    for (const auto& channel : sound.getChannels()) {
        const auto& data = channel.getRawSampleData();
        ctx.update(data.data(), data.size());
    }
    return ctx.get();
}

static auto CalcChannelInfoSize(const sound::Sound& sound) -> size_t {
    return std::transform_reduce(
        sound.getChannels().begin(), sound.getChannels().end(), sound.getChannelCount() * sizeof(resource::ResChannelInfo), std::plus<>(),
        [](const sound::Channel& channel) -> size_t {
            return sizeof(resource::ResSeekPoint) * channel.getSeekPointCount();
        }
    );
}

static constexpr auto AlignUp(size_t value) -> size_t {
    return common::AlignUp(value, 0x40ull);
}

auto BwavWriter::WriteChannel(const sound::Channel& channel, common::BinaryWriter& writer, size_t& dataOffset, sound::StreamInfo* streamInfo) -> bool {
    if (streamInfo != nullptr) {
        streamInfo->sampleCounts.emplace_back(channel.getSampleCount());
        streamInfo->sampleOffsets.emplace_back(static_cast<std::uint32_t>(dataOffset));
    }
    
    writer.write(ConvertSampleFormat(channel.getSampleFormat()));
    writer.write(ConvertOutputChannel(channel.getOutputChannel()));
    writer.write(channel.getSampleRate());
    writer.write(channel.getSampleCount()); // non-prefetch
    writer.write(channel.getSampleCount());
    switch (channel.getSampleFormat()) {
        case sound::Format::PcmInt16:
            writer.skip(0x20);
            break;
        case sound::Format::DspAdpcm:
            writer.write(channel.getAdpcmParameter());
            break;
        case sound::Format::Opus:
            writer.skip(0x20);
            break;
        default:
            return false;
    }
    writer.write(static_cast<std::uint32_t>(dataOffset)); // non-prefetch
    writer.write(static_cast<std::uint32_t>(dataOffset));
    dataOffset = AlignUp(dataOffset + channel.getRawSampleData().size());
    writer.write(static_cast<std::uint16_t>(channel.getSeekPointCount()));
    writer.write(static_cast<std::uint16_t>(channel.getLoopSeekPointIndex()));
    writer.write(channel.getLoopEnd());

    auto seekPoints = streamInfo != nullptr ? &streamInfo->seekPoints.emplace_back() : nullptr;
    for (const auto& point : channel.getSeekPoints()) {
        writer.write(point);
        if (channel.getSampleFormat() == sound::Format::DspAdpcm) {
            auto decoder = decode::CreateStreamDecoder(sound::Format::DspAdpcm);
            if (decoder->initialize(channel, writer.getEndian())) {
                decoder->seek(point);
                const auto& ctx = static_cast<decode::AdpcmStreamDecoder*>(decoder.get())->getContext();
                writer.write(ctx.predictorScale);
                writer.write(ctx.history[0]);
                writer.write(ctx.history[1]);
                writer.skip(2);
                if (seekPoints != nullptr) {
                    seekPoints->emplace(point, ctx);
                }
            } else {
                writer.skip(sizeof(resource::ResSeekPoint) - sizeof(std::uint32_t));
                if (seekPoints != nullptr) {
                    seekPoints->emplace(point, resource::AdpcmContext());
                }
            }
        } else {
            writer.skip(sizeof(resource::ResSeekPoint) - sizeof(std::uint32_t));
            if (seekPoints != nullptr) {
                seekPoints->emplace(point, resource::AdpcmContext());
            }
        }
    }

    return true;
}

auto BwavWriter::Write(const sound::Sound& sound, common::BinaryWriter& writer, sound::StreamInfo* streamInfo) -> bool {
    const auto dataHash = CalcDataHash(sound);
    if (streamInfo != nullptr) {
        streamInfo->dataHash = dataHash;
    }

    writer.writeArrayFixed<char, 4>({ 'B', 'W', 'A', 'V' });
    writer.write(static_cast<std::uint16_t>(0xfeff)); // bom
    writer.write(resource::ResBinaryWaveform::cVersion);
    writer.write(dataHash);
    writer.write(ConvertAssetType(sound.getAssetType()));
    writer.write(static_cast<std::uint16_t>(sound.getChannelCount()));

    auto dataOffset = AlignUp(CalcChannelInfoSize(sound));
    for (const auto& channel : sound.getChannels()) {
        if (!WriteChannel(channel, writer, dataOffset, streamInfo)) {
            return false;
        }
    }

    for (const auto& channel : sound.getChannels()) {
        if (channel.getRawSampleData().size() > 0) {
            writer.alignUp(0x40);
            writer.writeArray(std::span<const std::uint8_t>{channel.getRawSampleData()});
        }
    }

    return true;
}

auto BwavWriter::Write(const sound::Sound& sound, std::vector<std::uint8_t>& buffer, sound::StreamInfo* streamInfo) -> bool {
    auto writer = common::BinaryWriter(sound.getEndian());
    if (!Write(sound, writer, streamInfo)) {
        return false;
    }
    buffer.swap(writer.buffer());
    return true;
}

auto BwavWriter::Write(const sound::Sound& sound, std::string_view path, sound::StreamInfo* streamInfo) -> bool {
    auto buffer = std::vector<std::uint8_t>{};
    if (!Write(sound, buffer, streamInfo)) {
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

auto BwavWriter::WriteChannelPrefetch(const sound::Channel& channel, common::BinaryWriter& writer, size_t& dataOffset, std::uint32_t sampleOffset,
                                        std::uint32_t sampleCount, const std::unordered_map<std::uint32_t, resource::AdpcmContext>& seekPoints) -> bool {
    if (!sound::IsWritableFormat(channel.getSampleFormat())) {
        return false;
    }
                                            
    writer.write(ConvertSampleFormat(channel.getSampleFormat()));
    writer.write(ConvertOutputChannel(channel.getOutputChannel()));
    writer.write(channel.getSampleRate());
    writer.write(sampleCount); // non-prefetch
    writer.write(channel.getSampleCount());
    switch (channel.getSampleFormat()) {
        case sound::Format::PcmInt16:
            writer.skip(0x20);
            break;
        case sound::Format::DspAdpcm:
            writer.write(channel.getAdpcmParameter());
            break;
        case sound::Format::Opus:
            writer.write(channel.getOpusBlendSampleCount());
            writer.write(static_cast<std::uint32_t>(dataOffset + channel.getRawSampleData().size()));
            break;
        default:
            return false;
    }
    writer.write(sampleOffset); // non-prefetch
    writer.write(static_cast<std::uint32_t>(dataOffset));
    dataOffset = AlignUp(dataOffset + channel.getRawSampleData().size());
    if (channel.getSampleFormat() == sound::Format::Opus && channel.getOpusBlendSampleCount() > 0) {
        dataOffset = AlignUp(dataOffset + channel.getOpusBlendSampleCount() * sizeof(std::int16_t));
    }
    writer.write(static_cast<std::uint16_t>(channel.getSeekPointCount()));
    writer.write(static_cast<std::uint16_t>(channel.getLoopSeekPointIndex()));
    writer.write(channel.getLoopEnd());

    for (const auto& point : channel.getSeekPoints()) {
        writer.write(point);
        if (channel.getSampleFormat() == sound::Format::DspAdpcm) {
            if (const auto res = seekPoints.find(point); res != seekPoints.end()) {
                const auto& ctx = res->second;
                writer.write(ctx.predictorScale);
                writer.write(ctx.history[0]);
                writer.write(ctx.history[1]);
                writer.skip(2);
            } else {
                writer.skip(sizeof(resource::ResSeekPoint) - sizeof(std::uint32_t));
            }
        } else {
            writer.skip(sizeof(resource::ResSeekPoint) - sizeof(std::uint32_t));
        }
    }

    return true;
}

auto BwavWriter::WritePrefetch(const sound::Sound& sound, const sound::StreamInfo& streamInfo, common::BinaryWriter& writer) -> bool {
    if (sound.getAssetType() == sound::AssetType::Invalid) {
        return false;
    }

    writer.setEndian(sound.getEndian());
    writer.writeArrayFixed<char, 4>({ 'B', 'W', 'A', 'V' });
    writer.write<std::uint16_t>(0xfeff); // bom
    writer.write(resource::ResBinaryWaveform::cVersion);
    writer.write(streamInfo.dataHash);
    writer.write(ConvertAssetType(sound.getAssetType()));
    writer.write(static_cast<std::uint16_t>(sound.getChannelCount()));

    auto dataOffset = AlignUp(CalcChannelInfoSize(sound));

    for (const auto& [index, channel] : std::views::enumerate(std::as_const(sound.getChannels()))) {
        if (!WriteChannelPrefetch(channel, writer, dataOffset, streamInfo.sampleOffsets[index],
                                    streamInfo.sampleCounts[index], streamInfo.seekPoints[index])) {
            return false;
        }
    }

    for (const auto& channel : sound.getChannels()) {
        if (channel.getRawSampleData().size() > 0) {
            writer.alignUp(0x40);
            writer.writeArray(std::span<const std::uint8_t>{channel.getRawSampleData()});
        }

        if (channel.getSampleFormat() == sound::Format::Opus && channel.getOpusBlendSampleCount() > 0) {
            writer.alignUp(0x40);
            writer.writeArray(std::span<const std::int16_t>(channel.getOpusBlendSamples()));
        }
    }

    return true;
}

auto BwavWriter::WritePrefetch(const sound::Sound& sound, const sound::StreamInfo& streamInfo, std::vector<std::uint8_t>& buffer) -> bool {
    auto writer = common::BinaryWriter();
    if (!WritePrefetch(sound, streamInfo, writer)) {
        return false;
    }
    buffer.swap(writer.buffer());
    return true;
}

} // namespace encode