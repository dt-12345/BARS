#include "common/reader.hpp"
#include "decode/wavReader.hpp"
#include "sound/channel.hpp"
#include "sound/sound.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <ranges>

namespace decode {

template <typename T>
requires std::integral<T>
static auto ReadInterleaved(std::vector<std::reference_wrapper<std::vector<std::uint8_t>>>& buffers, common::BinaryReader& reader, std::uint32_t sampleCount) -> void {
    for (const auto sample : std::views::iota(0u, sampleCount)) {
        for (auto& buffer : buffers) {
            const auto out = buffer.get().data() + sample * sizeof(T);
            if constexpr (std::is_same_v<T, std::int16_t>) {
                *reinterpret_cast<T*>(out) = reader.read<T>();
            } else {
                const auto normalized = static_cast<float>(reader.read<T>()) / static_cast<float>(std::numeric_limits<T>::max());
                *reinterpret_cast<std::int16_t*>(out) = static_cast<std::int16_t>(normalized * static_cast<float>(std::numeric_limits<T>::max()));
            }
        }
    }
}

auto WavReader::Read(common::BinaryReader& reader, sound::Sound& sound) -> Result {
    reader.setEndian(std::endian::little);
    const auto base = reader.tell();

    if (!reader.checkSize(8)) {
        return BufferTooSmall;
    }

    const auto chunkId = reader.readArrayFixed<char, 4>();
    if (chunkId[0] != 'R' || chunkId[1] != 'I' || chunkId[2] != 'F' || chunkId[3] != 'F') {
        return InvalidWavMagic;
    }

    const auto chunkSize = reader.read<std::uint32_t>();
    if (!reader.checkSize(chunkSize)) {
        return BufferTooSmall;
    }

    const auto wavMagic = reader.readArrayFixed<char, 4>();
    if (wavMagic[0] != 'W' || wavMagic[1] != 'A' || wavMagic[2] != 'V' || wavMagic[3] != 'E') {
        return InvalidWavMagic;
    }

    auto foundFormatChunk = false;
    auto foundDataChunk = false;
    auto sampleRate = 0u;
    auto bitsPerSample = 0u;
    auto handleChunk = [&]() -> Result {
        if (!reader.checkSize(8)) {
            return BufferTooSmall;
        }

        const auto chunkId = reader.read<std::uint32_t>();

        const auto chunkSize = reader.read<std::uint32_t>();
        if (!reader.checkSize(chunkSize)) {
            return BufferTooSmall;
        }

        // TODO: cue point and playlist chunks for loops?
        switch (chunkId) {
            case 0x20746d66: { // "fmt "
                foundFormatChunk = true;
                if (chunkSize != 16 && chunkSize != 18 && chunkSize != 40) {
                    return InvalidWavChunk;
                }

                const auto fmt = reader.read<std::uint16_t>();
                if (fmt != 1) { // pcm
                    return UnsupportedWavFormat;
                }

                sound.setChannelCount(reader.read<std::uint16_t>());
                sampleRate = reader.read<std::uint32_t>();
                reader.skip(6); // avg bytes per sec, block size
                // assume pcm from here
                bitsPerSample = reader.read<std::uint16_t>();
                if (bitsPerSample != 16 && bitsPerSample != 32) {
                    return UnsupportedWavFormat;
                }
                break;
            }
            case 0x61746164: { // "data"
                if (!foundFormatChunk) {
                    return MissingWavChunk;
                }
                foundDataChunk = true;
                const auto sampleCount = chunkSize / sound.getChannelCount() / sizeof(std::int16_t);
                auto channelSamples = std::vector<std::reference_wrapper<std::vector<std::uint8_t>>>{};
                // we assume 16-bit pcm because that's what we had above
                for (auto& channel : sound.getChannels()) {
                    channel.setSampleRate(sampleRate);
                    channel.setSampleCountRaw(sampleCount);
                    channel.setSampleFormat(sound::Format::PcmInt16);
                    auto seekPoints = std::vector<std::uint32_t>{ 0u };
                    channel.setLoopInfo(seekPoints, 0, -1);
                    channel.getRawSampleData().clear();
                    channel.getRawSampleData().resize(sampleCount * sizeof(std::int16_t));
                    channelSamples.emplace_back(std::ref(channel.getRawSampleData()));
                }

                switch (sound.getChannelCount()) {
                    case 1:
                        sound.getChannels()[0].setOutputChannel(sound::ChannelPan::Center);
                        break;
                    case 2:
                        sound.getChannels()[0].setOutputChannel(sound::ChannelPan::Left);
                        sound.getChannels()[1].setOutputChannel(sound::ChannelPan::Right);
                        break;
                    case 3:
                        sound.getChannels()[0].setOutputChannel(sound::ChannelPan::Left);
                        sound.getChannels()[1].setOutputChannel(sound::ChannelPan::Right);
                        sound.getChannels()[2].setOutputChannel(sound::ChannelPan::Center);
                        break;
                    case 4:
                        sound.getChannels()[0].setOutputChannel(sound::ChannelPan::Left);
                        sound.getChannels()[1].setOutputChannel(sound::ChannelPan::Right);
                        sound.getChannels()[2].setOutputChannel(sound::ChannelPan::LeftSurround);
                        sound.getChannels()[3].setOutputChannel(sound::ChannelPan::RightSurround);
                        break;
                    case 5:
                        sound.getChannels()[0].setOutputChannel(sound::ChannelPan::Left);
                        sound.getChannels()[1].setOutputChannel(sound::ChannelPan::Right);
                        sound.getChannels()[2].setOutputChannel(sound::ChannelPan::Center);
                        sound.getChannels()[3].setOutputChannel(sound::ChannelPan::LeftSurround);
                        sound.getChannels()[4].setOutputChannel(sound::ChannelPan::RightSurround);
                        break;
                    case 6:
                        sound.getChannels()[0].setOutputChannel(sound::ChannelPan::Left);
                        sound.getChannels()[1].setOutputChannel(sound::ChannelPan::Right);
                        sound.getChannels()[2].setOutputChannel(sound::ChannelPan::Center);
                        sound.getChannels()[3].setOutputChannel(sound::ChannelPan::LowFrequencyEffect);
                        sound.getChannels()[4].setOutputChannel(sound::ChannelPan::LeftSurround);
                        sound.getChannels()[5].setOutputChannel(sound::ChannelPan::RightSurround);
                        break;
                    default:
                        for (auto& channel : sound.getChannels()) {
                            channel.setOutputChannel(sound::ChannelPan::Center);
                        }
                        break;
                }

                if (bitsPerSample == 16) {
                    ReadInterleaved<std::int16_t>(channelSamples, reader, sampleCount);
                } else if (bitsPerSample == 32) {
                    ReadInterleaved<std::int32_t>(channelSamples, reader, sampleCount);
                }
                break;
            }
            default:
                reader.skip(chunkSize);
                break;
        }

        return OK;
    };

    while (reader.tell() < base + chunkSize) {
        if (const auto res = handleChunk(); res != OK) {
            return res;
        }
    }

    if (!foundFormatChunk || !foundDataChunk) {
        return MissingWavChunk;
    }

    sound.setAssetType(sound::AssetType::Normal);
    return OK;
}

// TODO: this is very crude, add support for more wave features later
// maybe it's better to have this return a wav-specific object and convert that?
auto WavReader::Read(common::BinaryReader& reader) -> std::expected<std::unique_ptr<sound::Sound>, Result> {
    auto sound = std::make_unique<sound::Sound>();
    if (const auto res = Read(reader, *sound); res != OK) {
        return std::expected<std::unique_ptr<sound::Sound>, Result>{ std::unexpected(res) };
    }
    return std::expected<std::unique_ptr<sound::Sound>, Result>{ std::in_place, std::move(sound) };
}

auto WavReader::Read(std::span<const std::uint8_t> data) -> std::expected<std::unique_ptr<sound::Sound>, Result> {
    auto reader = common::BinaryReader(std::move(data));
    return Read(reader);
}

auto WavReader::Read(std::string_view filepath) -> std::expected<std::unique_ptr<sound::Sound>, Result> {
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

    return Read(buffer);
}

} // namespace decode