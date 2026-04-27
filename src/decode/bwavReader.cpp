#include "decode/bwavReader.hpp"
#include "resource/adpcm.hpp"
#include "resource/bwav.hpp"
#include "resource/opus.hpp"

#include <filesystem>
#include <fstream>

#define OPUS_SIZE_DETERMINATION_ERROR 0xffff'ffffu

namespace decode {

static constexpr auto ConvertSampleFormat(resource::SampleFormat fmt) -> sound::Format {
    switch (fmt) {
        case resource::SampleFormat::PcmInt16:
            return sound::Format::PcmInt16;
        case resource::SampleFormat::Adpcm:
            return sound::Format::DspAdpcm;
        case resource::SampleFormat::Opus:
            return sound::Format::Opus;
        default:
            return sound::Format::PcmInt16;
    }
}

static constexpr auto ConvertOutputChannel(resource::OutputChannel ch) -> sound::ChannelPan {
    switch (ch) {
        case resource::OutputChannel::Left:
            return sound::ChannelPan::Left;
        case resource::OutputChannel::Right:
            return sound::ChannelPan::Right;
        case resource::OutputChannel::Center:
            return sound::ChannelPan::Center;
        case resource::OutputChannel::LowFrequencyEffect:
            return sound::ChannelPan::LowFrequencyEffect;
        case resource::OutputChannel::LeftSurround:
            return sound::ChannelPan::LeftSurround;
        case resource::OutputChannel::RightSurround:
            return sound::ChannelPan::RightSurround;
        default:
            return sound::ChannelPan::Center;
    }
}

static constexpr auto ConvertAssetType(resource::AssetType type) -> sound::AssetType {
    switch (type) {
        case resource::AssetType::Normal:
            return sound::AssetType::Normal;
        case resource::AssetType::Prefetch:
            return sound::AssetType::Prefetch;
        default:
            return sound::AssetType::Invalid;
    }
}

[[nodiscard]]
static auto GetOpusDataByteSize(common::BinaryReader& reader) -> std::uint32_t {
    const auto origOffset = reader.tell();
    const auto headerChunkType = reader.read<resource::ChunkType>();
    if (headerChunkType != resource::ChunkType::Header) {
        return OPUS_SIZE_DETERMINATION_ERROR;
    }
    const auto headerSize = reader.read<std::uint32_t>();
    reader.skip(offsetof(resource::OpusHeader, dataOffset));
    const auto dataChunkOffset = reader.read<std::uint32_t>();
    if (dataChunkOffset != 0x20) {
        return OPUS_SIZE_DETERMINATION_ERROR;
    }
    reader.seek(origOffset + dataChunkOffset);
    if (!reader.checkSize(sizeof(resource::OpusChunk))) {
        return OPUS_SIZE_DETERMINATION_ERROR;
    }
    const auto dataChunkType = reader.read<resource::ChunkType>();
    if (dataChunkType != resource::ChunkType::Data) {
        return OPUS_SIZE_DETERMINATION_ERROR;
    }
    const auto dataSize = reader.read<std::uint32_t>();
    reader.seek(origOffset);
    return sizeof(resource::OpusChunk) + headerSize + sizeof(resource::OpusChunk) + dataSize;
}

auto BwavReader::ReadChannel(common::BinaryReader& reader, sound::Channel& channel, size_t baseOffset, bool isPrefetch, sound::StreamInfo* streamInfo) -> Result {
    if (!reader.checkSize(sizeof(resource::ResChannelInfo))) {
        return BufferTooSmall;
    }

    channel.setSampleFormat(ConvertSampleFormat(reader.read<resource::SampleFormat>()));
    channel.setOutputChannel(ConvertOutputChannel(reader.read<resource::OutputChannel>()));
    channel.setSampleRate(reader.read<std::uint32_t>());
    const auto totalSampleCount = reader.read<std::uint32_t>();
    channel.setSampleCountRaw(reader.read<std::uint32_t>());
    switch (channel.getSampleFormat()) {
        case sound::Format::PcmInt16:
            reader.skip(0x20);
            break;
        case sound::Format::DspAdpcm: {
            channel.setAdpcmParameter(reader.read<resource::AdpcmParameter>());
            break;
        }
        case sound::Format::Opus:
            if (isPrefetch) {
                const auto blendSampleCount = reader.read<std::uint32_t>();
                const auto blendSampleOffset = reader.read<std::uint32_t>();
                reader.skip(0x18);
                auto blendSamples = reader.readArray<std::int16_t>(baseOffset + blendSampleOffset, blendSampleCount);
                channel.setOpusBlendSamples(blendSamples);
            } else {
                reader.skip(0x20);
            }
            break;
        default:
            return InvalidFormat;
    }
    const auto totalSampleOffset = reader.read<std::uint32_t>();
    const auto sampleOffset = reader.read<std::uint32_t>();
    const auto seekPointCount = reader.read<std::uint16_t>();
    const auto baseSeekPointIndex = reader.read<std::uint16_t>();
    const auto end = reader.read<std::int32_t>();

    if (!reader.checkSize(sizeof(resource::ResSeekPoint) * seekPointCount)) {
        return BufferTooSmall;
    }

    auto seekPoints = std::vector<std::uint32_t>(seekPointCount);
    auto seekPointsForPrefetch = streamInfo != nullptr ? &streamInfo->seekPoints.emplace_back() : nullptr;
    for (auto& point : seekPoints) {
        point = reader.read<std::uint32_t>();
        // adpcm loop context
        if (seekPointsForPrefetch != nullptr) {
            auto ctx = resource::AdpcmContext();
            ctx.predictorScale = reader.read<std::uint16_t>();
            ctx.history[0] = reader.read<std::int16_t>();
            ctx.history[1] = reader.read<std::int16_t>();
            seekPointsForPrefetch->emplace(point, std::move(ctx));
            reader.skip(2);
        } else {
            reader.skip(8);
        }
    }
    channel.setLoopInfo(seekPoints, baseSeekPointIndex, end);

    const auto returnOffset = reader.tell();
    reader.seek(baseOffset + sampleOffset);
    switch (channel.getSampleFormat()) {
        case sound::Format::PcmInt16: {
            auto samples = reader.tryReadArray<std::uint8_t>(channel.getSampleCount() * sizeof(std::int16_t));
            if (!samples) {
                return BufferTooSmall;
            }
            channel.setSamples(*samples);
            break;
        }
        case sound::Format::DspAdpcm: {
            const auto sampleByteSize = resource::CalcAdpcmSampleByteSize(channel.getSampleCount());
            auto samples = reader.tryReadArray<std::uint8_t>(end == -1 ? common::AlignUp(sampleByteSize, 8u) : sampleByteSize);
            if (!samples) {
                return BufferTooSmall;
            }
            channel.setSamples(*samples);
            break;
        }
        case sound::Format::Opus: {
            if (isPrefetch) {
                auto samples = reader.tryReadArray<std::uint8_t>(channel.getSampleCount() * sizeof(std::int16_t));
                if (!samples) {
                    return BufferTooSmall;
                }
                channel.setSamples(*samples);
                break;
            } else {
                if (!reader.checkSize(sizeof(resource::OpusChunk) + sizeof(resource::OpusHeader))) {
                    return BufferTooSmall;
                }
                const auto sampleByteSize = GetOpusDataByteSize(reader);
                if (sampleByteSize == OPUS_SIZE_DETERMINATION_ERROR) {
                    return CannotDetermineOpusSize;
                }
                auto samples = reader.tryReadArray<std::uint8_t>(sampleByteSize);
                if (!samples) {
                    return BufferTooSmall;
                }
                channel.setSamples(*samples);
                break;
            }
        }
        default:
            return InvalidFormat;
    }

    if (streamInfo != nullptr) {
        streamInfo->sampleOffsets.emplace_back(totalSampleOffset);
        streamInfo->sampleCounts.emplace_back(totalSampleCount);
    }

    reader.seek(returnOffset);
    return OK;
}

auto BwavReader::Read(common::BinaryReader& reader, sound::Sound& sound, sound::StreamInfo* streamInfo) -> Result {
    if (!reader.checkSize(sizeof(resource::ResBinaryWaveform))) {
        return BufferTooSmall;
    }

    const auto baseOffset = reader.tell();
    const auto magic = reader.readArrayFixed<char, 4>();
    if (magic[0] != 'B' || magic[1] != 'W' || magic[2] != 'A' || magic[3] != 'V') {
        return InvalidBwavMagic;
    }

    const auto bom = reader.read<std::uint16_t>(std::endian::big);
    const auto endian = bom == 0xfffe ? std::endian::little : std::endian::big;
    reader.setEndian(endian);

    const auto version = reader.read<std::uint16_t>();
    if (version != resource::ResBinaryWaveform::cVersion) {
        return InvalidBwavVersion;
    }

    const auto dataHash = reader.read<std::uint32_t>();
    if (streamInfo != nullptr) {
        streamInfo->dataHash = dataHash;
    }

    const auto assetType = ConvertAssetType(reader.read<resource::AssetType>());
    if (assetType != sound::AssetType::Normal && assetType != sound::AssetType::Prefetch) {
        return InvalidAssetType;
    }

    sound.setAssetType(assetType);
    sound.setEndian(endian);
    sound.setChannelCount(reader.read<std::uint16_t>());
    for (auto& channel : sound.getChannels()) {
        if (const auto res = ReadChannel(reader, channel, baseOffset, assetType == sound::AssetType::Prefetch, streamInfo); res != OK) {
            return res;
        }
    }

    return OK;
}

auto BwavReader::Read(common::BinaryReader& reader, sound::StreamInfo* streamInfo) -> std::expected<std::unique_ptr<sound::Sound>, Result> {
    auto sound = std::make_unique<sound::Sound>();
    if (const auto res = Read(reader, *sound, streamInfo); res != OK) {
        return std::expected<std::unique_ptr<sound::Sound>, Result>{ std::unexpected(res) };
    }
    return std::expected<std::unique_ptr<sound::Sound>, Result>{ std::in_place, std::move(sound) };
}

auto BwavReader::Read(std::span<const std::uint8_t> data, sound::StreamInfo* streamInfo) -> std::expected<std::unique_ptr<sound::Sound>, Result> {
    auto reader = common::BinaryReader(std::move(data));
    return Read(reader, streamInfo);
}

auto BwavReader::Read(std::string_view filepath, sound::StreamInfo* streamInfo) -> std::expected<std::unique_ptr<sound::Sound>, Result> {
    const auto path = std::filesystem::path(filepath);
    if (!std::filesystem::exists(path)) {
        return std::expected<std::unique_ptr<sound::Sound>, Result>{ std::unexpected(FileNotFound) };
    }

    const auto size = std::filesystem::file_size(path);
    auto infile = std::ifstream(path, std::ios::binary);
    if (!infile) {
        return std::expected<std::unique_ptr<sound::Sound>, Result>{ std::unexpected(FileNotFound) };
    }

    auto buffer = std::vector<std::uint8_t>(size);
    infile.read(reinterpret_cast<char*>(buffer.data()), size);

    return Read(buffer, streamInfo);
}

} // namespace decode