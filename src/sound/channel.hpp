#pragma once

#include "common/align.hpp"
#include "sound/types.hpp"
#include "resource/adpcm.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sound {

struct StreamInfo {
    std::vector<std::unordered_map<std::uint32_t, resource::AdpcmContext>> seekPoints;
    std::vector<std::uint32_t> sampleOffsets;
    std::vector<std::uint32_t> sampleCounts;
    std::uint32_t dataHash;
};

class Channel {
public:
    Channel();

    // takes ownership of samples data
    auto setSamples(std::vector<std::uint8_t>& samples) -> void { mSamples.swap(samples); }
    auto setPrefetchSampleCount(std::uint32_t samples) -> void { mPrefetchSampleCount = samples; }
    // warning: this does not affect sample data
    auto setSampleCountRaw(std::uint32_t samples) -> void { mSampleCount = samples; }
    // warning: this does not resample
    auto setSampleRate(std::uint32_t rate) -> void { mSampleRate = rate; }
    auto setSampleFormat(Format fmt) -> void { mSampleFormat = fmt; }
    auto setOutputChannel(ChannelPan pan) -> void { mOutputChannel = pan; }
    // takes ownership of seekPoints data
    auto setLoopInfo(std::vector<std::uint32_t>& seekPoints, std::uint16_t loopSeekPointIndex, std::int32_t loopEnd) -> void;
    auto setLoopEnd(std::int32_t loopEnd) -> void { mLoopEnd = loopEnd; }
    auto setLoopPoint(std::uint32_t loopPoint) -> void;
    // takes ownership of samples data
    auto setOpusBlendSamples(std::vector<std::int16_t>& samples) -> void { mOpusBlendSamples.swap(samples); }
    auto setAdpcmParameter(const resource::AdpcmParameter& param) -> void { mAdpcmParameter = param; }

    [[nodiscard]] auto getPrefetchSampleCount() const -> std::uint32_t { return mPrefetchSampleCount; }
    [[nodiscard]] auto getSampleCount() const -> std::uint32_t { return mSampleCount; }
    [[nodiscard]] auto getRawSampleData() -> std::vector<std::uint8_t>& { return mSamples; }
    [[nodiscard]] auto getRawSampleData() const -> const std::vector<std::uint8_t>& { return mSamples; }
    [[nodiscard]] auto getSampleRate() const -> std::uint32_t { return mSampleRate; }
    [[nodiscard]] auto getSampleFormat() const -> Format { return mSampleFormat; }
    [[nodiscard]] auto getOutputChannel() const -> ChannelPan { return mOutputChannel; }
    [[nodiscard]] auto getSeekPointCount() const -> std::uint32_t { return static_cast<std::uint32_t>(mSeekPoints.size()); }
    [[nodiscard]] auto getLoopEnd() const -> std::int32_t { return mLoopEnd; }
    [[nodiscard]] auto getLoopSeekPointIndex() const -> std::uint16_t { return mLoopSeekPointIndex; }
    [[nodiscard]] auto getSeekPoints() -> std::vector<std::uint32_t>& { return mSeekPoints; }
    [[nodiscard]] auto getSeekPoints() const -> const std::vector<std::uint32_t>& { return mSeekPoints; }
    [[nodiscard]] auto getOpusBlendSampleCount() const -> std::uint32_t { return static_cast<std::uint32_t>(mOpusBlendSamples.size()); }
    [[nodiscard]] auto getOpusBlendSamples() -> std::vector<std::int16_t>& { return mOpusBlendSamples; }
    [[nodiscard]] auto getOpusBlendSamples() const -> const std::vector<std::int16_t>& { return mOpusBlendSamples; }
    [[nodiscard]] auto getAdpcmParameter() -> resource::AdpcmParameter& { return mAdpcmParameter; }
    [[nodiscard]] auto getAdpcmParameter() const -> const resource::AdpcmParameter& { return mAdpcmParameter; }

    auto createPrefetch(Channel& prefetch, std::endian endian) const -> bool;
    // copy prefetch channel info over to non-prefetch channel
    auto setPrefetchInfo(const Channel& prefetch) -> void;
    
    auto requiresStreaming() const -> bool;
    
    // also stores blend samples for opus if applicable
    auto resampleAndConvert(Format fmt, std::uint32_t sampleRate, std::uint32_t sampleCount, std::endian endian) -> bool;
    auto convert(Format fmt, std::endian endian) -> bool { return resampleAndConvert(fmt, mSampleRate, mSampleCount, endian); }
    auto resample(std::uint32_t sampleRate, std::endian endian) -> bool { return resampleAndConvert(mSampleFormat, sampleRate, mSampleCount, endian); }
    auto setSampleCount(std::uint32_t sampleCount, std::endian endian) -> bool { return resampleAndConvert(mSampleFormat, mSampleRate, sampleCount, endian); }
    auto extend(std::uint32_t additionalSamples, std::endian endian) -> bool { return setSampleCount(mSampleCount + additionalSamples, endian); }
    auto cut(std::uint32_t samples, std::endian endian) -> bool { return setSampleCount(mSampleCount - std::min(samples, mSampleCount), endian); }
    auto alignUp(std::uint32_t align, std::endian endian) -> bool { return setSampleCount(common::AlignUp(mSampleCount, align), endian); }

private:
    std::vector<std::uint8_t> mSamples;
    std::uint32_t mPrefetchSampleCount;
    std::uint32_t mSampleCount; // sample count for this sample
    std::uint32_t mSampleRate;
    Format mSampleFormat;
    ChannelPan mOutputChannel;
    std::int32_t mLoopEnd;
    std::uint16_t mLoopSeekPointIndex;
    std::vector<std::uint32_t> mSeekPoints; // should we store the loop context or just recalculate it ourselves?
    std::vector<std::int16_t> mOpusBlendSamples;
    resource::AdpcmParameter mAdpcmParameter;
};

} // namespace sound