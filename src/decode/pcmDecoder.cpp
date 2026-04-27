#include "common/endian.hpp"
#include "decode/pcmDecoder.hpp"
#include "sound/channel.hpp"

#include <algorithm>
#include <cstring>

namespace decode {

auto Pcm16Decoder::initialize(const sound::Channel& channel [[maybe_unused]], std::endian endian) -> bool {
    mEndian = endian;
    return true;
}

auto Pcm16Decoder::decode(std::span<const std::uint8_t> inBuf, std::span<std::int16_t> outBuf) -> std::uint32_t {
    const auto maxSamples = inBuf.size() / sizeof(std::int16_t);
    const auto numSamples = std::min(maxSamples, outBuf.size());

    if (mEndian == std::endian::native) {
        std::memcpy(outBuf.data(), inBuf.data(), numSamples * sizeof(std::int16_t));
    } else {
        const auto range = std::span{ reinterpret_cast<const std::int16_t*>(inBuf.data()), numSamples };
        std::transform(range.cbegin(), range.cend(), outBuf.begin(), common::ByteSwap<std::int16_t>);
    }
    return static_cast<std::uint32_t>(numSamples);
}

auto Pcm16StreamDecoder::initialize(const sound::Channel& channel, std::endian endian) -> bool {
    mOutputChannel = channel.getOutputChannel();
    mData = channel.getRawSampleData();
    mOffset = 0;
    mEndian = endian;
    return true;
}

auto Pcm16StreamDecoder::read(std::span<std::int16_t> outBuf) -> std::uint32_t {
    if (mOffset >= mData.size()) {
        return 0;
    }

    const auto numSamples = std::min(outBuf.size(), (mData.size() - mOffset) / sizeof(std::int16_t));
    if (mEndian == std::endian::native) {
        std::memcpy(outBuf.data(), mData.data() + mOffset, numSamples * sizeof(std::int16_t));
    } else {
        const auto range = std::span{ reinterpret_cast<const std::int16_t*>(mData.data() + mOffset), numSamples };
        std::transform(range.cbegin(), range.cend(), outBuf.begin(), common::ByteSwap<std::int16_t>);
    }
    mOffset += numSamples * sizeof(std::int16_t);
    return numSamples;
}

auto Pcm16StreamDecoder::seek(std::uint32_t samplePos) -> void {
    if (samplePos * sizeof(std::int16_t) >= mData.size()) {
        mOffset = mData.size();
        return;
    }

    mOffset = samplePos * sizeof(std::int16_t);
}

auto Pcm16StreamDecoder::tell() const -> std::uint32_t {
    return mOffset / sizeof(std::int16_t);
}

auto Pcm16StreamDecoder::getSampleCount() const -> std::uint32_t {
    return static_cast<std::uint32_t>(mData.size()) / sizeof(std::int16_t);
}

} // namespace decode