#pragma once

#include "decode/decoder.hpp"
#include "resource/adpcm.hpp"

namespace decode {

class AdpcmDecoder : public IDecoder {
public:
    AdpcmDecoder() = default;

    ~AdpcmDecoder() override = default;
    [[nodiscard]] auto initialize(const sound::Channel& channel, std::endian endian) -> bool override;
    [[nodiscard]] auto decode(std::span<const std::uint8_t> inBuf, std::span<std::int16_t> ) -> std::uint32_t override;

private:
    resource::AdpcmParameter mParameter;
};

class AdpcmStreamDecoder : public IStreamDecoder {
public:
    AdpcmStreamDecoder() = default;

    ~AdpcmStreamDecoder() override = default;
    [[nodiscard]] auto initialize(const sound::Channel& channel, std::endian endian) -> bool override;
    [[nodiscard]] auto read(std::span<std::int16_t> outBuf) -> std::uint32_t override;
    auto seek(std::uint32_t samplePos) -> void override;
    [[nodiscard]] auto tell() const -> std::uint32_t override;
    [[nodiscard]] auto getSampleCount() const -> std::uint32_t override;
    [[nodiscard]] auto getOutputChannel() const -> sound::ChannelPan override { return mOutputChannel; }

    [[nodiscard]] auto getContext() const -> const resource::AdpcmContext& { return mContext; }

private:
    auto isStartOfFrame() const -> bool { return mFrameOffset == 0; }
    auto readPredictorScale() -> void;
    auto decodeSample() -> std::int16_t;

    resource::AdpcmParameter mParameter;
    resource::AdpcmContext mContext;
    std::uint16_t mFrameOffset;
    std::span<const std::uint8_t> mData;
    size_t mFrameIndex;
    sound::ChannelPan mOutputChannel;
};

} // namespace decode