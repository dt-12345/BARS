#include "decode/adpcmDecoder.hpp"
#include "resource/adpcm.hpp"
#include "sound/channel.hpp"

#include <algorithm>
#include <limits>

namespace decode {

auto AdpcmDecoder::initialize(const sound::Channel& channel, std::endian endian [[maybe_unused]]) -> bool {
    mParameter = channel.getAdpcmParameter();
    return true;
}

static inline auto ExtractSignedHighNibble(const std::uint8_t* samples) -> std::int32_t {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(*samples >> 4) << 0x1c) >> 0x1c;
}

static inline auto ExtractSignedLowNibble(const std::uint8_t* samples) -> std::int32_t {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(*samples & 0xf) << 0x1c) >> 0x1c;
}

auto AdpcmDecoder::decode(std::span<const std::uint8_t> inBuf, std::span<std::int16_t> outBuf) -> std::uint32_t {
    if (inBuf.size() < 2) {
        return 0;
    }

    // each frame is 8 bytes, 1 byte header + 7 bytes of 4 bit samples
    const auto maxSamples = static_cast<size_t>(resource::CalcAdpcmSampleCountFromByteSize(inBuf.size()));
    const auto numSamples = static_cast<std::uint32_t>(std::min(maxSamples, outBuf.size()));

    auto hist1 = std::int16_t(0);
    auto hist2 = std::int16_t(0);
    const auto frameCount = (numSamples + resource::cAdpcmSamplesPerFrame - 1) / resource::cAdpcmSamplesPerFrame;
    auto remaining = numSamples;
    auto samplesIn = inBuf.data();
    auto samplesOut = outBuf.data();
    for (std::uint32_t i = 0; i < frameCount; ++i) {
        const auto predictor = static_cast<std::uint32_t>(*samplesIn >> 4);
        const auto scale = 1 << static_cast<std::int32_t>(*samplesIn++ & 0xf);
        const auto coef1 = mParameter.coefficients[predictor][0];
        const auto coef2 = mParameter.coefficients[predictor][1];

        const auto samplesInFrame = remaining < resource::cAdpcmSamplesPerFrame ? remaining : resource::cAdpcmSamplesPerFrame;
        for (std::uint32_t j = 0; j < samplesInFrame; ++j) {
            const auto rawSample = (j & 1) ? ExtractSignedLowNibble(samplesIn++) : ExtractSignedHighNibble(samplesIn);
            const auto sample = (rawSample * 0x800 * scale + 0x400 + coef1 * hist1 + coef2 * hist2) >> 0xb;
            const auto finalSample = static_cast<std::int16_t>(std::clamp(
                sample,
                static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
                static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max())
            ));

            hist2 = hist1;
            hist1 = finalSample;
            *samplesOut++ = finalSample;
        }

        remaining -= samplesInFrame;
    }
    return numSamples;
}

auto AdpcmStreamDecoder::readPredictorScale() -> void {
    mContext.predictorScale = static_cast<std::uint16_t>(mData[mFrameIndex * 8]);
}

auto AdpcmStreamDecoder::decodeSample() -> std::int16_t {
    if (isStartOfFrame()) {
        readPredictorScale();
    }
    const auto coef1 = mParameter.coefficients[mContext.getPredictor()][0];
    const auto coef2 = mParameter.coefficients[mContext.getPredictor()][1];

    const auto rawSample = (mFrameOffset & 1)
        ? ExtractSignedLowNibble(mData.data() + mFrameIndex * 8 + mFrameOffset++ / 2 + 1)
        : ExtractSignedHighNibble(mData.data() + mFrameIndex * 8 + mFrameOffset++ / 2 + 1);

    const auto sample = (rawSample * 0x800 * mContext.getScale() + 0x400 + coef1 * mContext.history[0] + coef2 * mContext.history[1]) >> 0xb;
    const auto finalSample = static_cast<std::int16_t>(std::clamp(
        sample,
        static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
        static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max())
    ));

    mContext.history[1] = mContext.history[0];
    mContext.history[0] = finalSample;
    if (mFrameOffset >= resource::cAdpcmSamplesPerFrame) {
        mFrameOffset = 0;
        ++mFrameIndex;
    }

    return finalSample;
}

auto AdpcmStreamDecoder::initialize(const sound::Channel& channel, std::endian endian [[maybe_unused]]) -> bool {
    mParameter = channel.getAdpcmParameter();
    mOutputChannel = channel.getOutputChannel();
    mData = channel.getRawSampleData();
    mFrameOffset = 0;
    mFrameIndex = 0;
    mContext.history[0] = 0;
    mContext.history[1] = 0;
    return true;
}

auto AdpcmStreamDecoder::read(std::span<std::int16_t> outBuf) -> std::uint32_t {
    if (mFrameIndex * 8 + mFrameOffset * 2 >= mData.size()) {
        return 0;
    }

    const auto maxRemainingSamples = getSampleCount() - tell();
    const auto numSamples = static_cast<std::uint32_t>(std::min(maxRemainingSamples, static_cast<std::uint32_t>(outBuf.size())));

    auto remaining = numSamples;
    auto samplesOut = outBuf.data();
    while (remaining > 0) {
        *samplesOut++ = decodeSample();
        --remaining;
    }
    return numSamples;
}

auto AdpcmStreamDecoder::seek(std::uint32_t samplePos) -> void {
    if (samplePos == tell()) {
        readPredictorScale();
        return;
    }

    const auto totalSampleCount = getSampleCount();
    if (samplePos >= totalSampleCount) {
        samplePos = totalSampleCount;
    }

    if (samplePos > tell()) {
        auto remaining = samplePos - tell();
        while (remaining > 0) {
            decodeSample();
            --remaining;
        }
    } else if (samplePos < tell()) {
        mFrameIndex = 0;
        mFrameOffset = 0;
        mContext.history[0] = 0;
        mContext.history[1] = 0;
        while (samplePos > 0) {
            decodeSample();
            --samplePos;
        }
    }

    readPredictorScale();
}

auto AdpcmStreamDecoder::tell() const -> std::uint32_t {
    return static_cast<std::uint32_t>(mFrameIndex) * resource::cAdpcmSamplesPerFrame + mFrameOffset;
}

auto AdpcmStreamDecoder::getSampleCount() const -> std::uint32_t  {
    return resource::CalcAdpcmSampleCountFromByteSize(static_cast<std::uint32_t>(mData.size()));
}

} // namespace decode