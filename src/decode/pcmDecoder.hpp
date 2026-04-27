#pragma once

#include "decode/decoder.hpp"

namespace decode {

class Pcm16Decoder : public IDecoder {
public:
    Pcm16Decoder() = default;

    ~Pcm16Decoder() override = default;
    [[nodiscard]] auto initialize(const sound::Channel& channel, std::endian endian) -> bool override;
    [[nodiscard]] auto decode(std::span<const std::uint8_t> inBuf, std::span<std::int16_t> outBuf) -> std::uint32_t override;

private:
    std::endian mEndian;
};

class Pcm16StreamDecoder : public IStreamDecoder {
public:
    Pcm16StreamDecoder() = default;

    ~Pcm16StreamDecoder() override = default;
    [[nodiscard]] auto initialize(const sound::Channel& channel, std::endian endian) -> bool override;
    [[nodiscard]] auto read(std::span<std::int16_t> outBuf) -> std::uint32_t override;
    auto seek(std::uint32_t samplePos) -> void override;
    [[nodiscard]] auto tell() const -> std::uint32_t override;
    [[nodiscard]] auto getSampleCount() const -> std::uint32_t override;
    [[nodiscard]] auto getOutputChannel() const -> sound::ChannelPan override { return mOutputChannel; }

private:
    std::span<const std::uint8_t> mData;
    size_t mOffset;
    std::endian mEndian;
    sound::ChannelPan mOutputChannel;
};

} // namespace decode