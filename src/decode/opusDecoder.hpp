#pragma once

#include "decode/decoder.hpp"
#include "decoder.hpp"

namespace decode {

class OpusDecoder : public IDecoder {
public:
    OpusDecoder() = default;

    ~OpusDecoder() override = default;
    [[nodiscard]] auto initialize(const sound::Channel& channel, std::endian endian) -> bool override;
    [[nodiscard]] auto decode(std::span<const std::uint8_t> inBuf, std::span<std::int16_t> outBuf) -> std::uint32_t override;
    [[nodiscard]] auto getSampleAlignment(std::span<const std::uint8_t> inBuf) const -> std::uint32_t override;

private:
    std::endian mEndian;
};

struct OpusStreamInfo {
    std::span<const std::int16_t> blendSamples;
    std::uint32_t prefetchSampleCount;
};

class OpusStreamDecoder : public IStreamDecoder {
public:
    OpusStreamDecoder() = default;

    ~OpusStreamDecoder() override;
    [[nodiscard]] auto initialize(const sound::Channel& channel, std::endian endian) -> bool override;
    [[nodiscard]] auto read(std::span<std::int16_t> outBuf) -> std::uint32_t override;
    auto seek(std::uint32_t samplePos) -> void override;
    [[nodiscard]] auto tell() const -> std::uint32_t override;
    [[nodiscard]] auto getSampleCount() const -> std::uint32_t override;
    [[nodiscard]] auto getOutputChannel() const -> sound::ChannelPan override { return mOutputChannel; }

private:
    [[nodiscard]] auto copyFrame(std::span<std::int16_t> outBuf) -> std::uint32_t;

    struct DecoderState;
    DecoderState* mState;
    std::span<const std::uint8_t> mData;
    size_t mOffset;
    sound::ChannelPan mOutputChannel;
};

} // namespace decode