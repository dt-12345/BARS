#pragma once

#include "decode/decoder.hpp"

#include <miniaudio.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace sound {
class Sound;
}

namespace playback {

// TODO: split into ChannelSource and SoundSource? is that better? it seems kind of wasteful, but each channel is configured separately in bwav
class SoundSource {
public:
    explicit SoundSource() = default;
    ~SoundSource();

    auto initialize(const sound::Sound& sound) -> bool;

    auto play() -> bool;
    auto pause() -> bool;
    auto stop() -> bool;
    auto seek(std::int32_t target, bool fromCurrent = false) -> std::uint32_t;
    [[nodiscard]] auto tell() const -> std::uint32_t;
    [[nodiscard]] auto getSampleRate() const -> std::uint32_t { return mSampleRate; }

    auto setLoop(bool isLooping) -> void;
    auto setLoopPoints(std::uint32_t loopStart, std::uint32_t loopEnd = cEndOfSamples) -> void;
    auto setRange(std::uint32_t start, std::uint32_t end = cEndOfSamples) -> void;

    auto setLoopPoints(float loopStart, float loopEnd) -> void { setLoopPoints(secToSample(loopStart), secToSample(loopEnd)); }
    auto setRange(float start, float end) -> void { setRange(secToSample(start), secToSample(end)); }

    auto secToSample(float time) const -> std::uint32_t { return static_cast<std::uint32_t>(time * static_cast<float>(mSampleRate)); }
    auto sampleToSec(std::uint32_t sample) const -> float { return static_cast<float>(sample) / static_cast<float>(mSampleRate); }

    static constexpr const auto cEndOfSamples = 0xffff'ffffu;

private:
    static constexpr const auto cInvalidSeekTarget = 0xffff'ffffu;

    static const ma_data_source_vtable sVtable;

    static auto dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) -> void;
    static auto dataSourceRead(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) -> ma_result;
    static auto dataSourceSeek(ma_data_source* pDataSource, ma_uint64 frameIndex) -> ma_result;
    static auto dataSourceGetDataFormat(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap) -> ma_result;
    static auto dataSourceGetCursor(ma_data_source* pDataSource, ma_uint64* pCursor) -> ma_result;
    static auto dataSourceGetLength(ma_data_source* pDataSource, ma_uint64* pLength) -> ma_result;
    static auto dataSourceOnSetLooping(ma_data_source* pDataSource, ma_bool32 isLooping) -> ma_result;

    ma_data_source_base mBase;
    ma_device mDevice;
    std::vector<std::unique_ptr<decode::IStreamDecoder>> mDecoders;
    std::vector<std::vector<std::int16_t>> mChannelBuffers;
    std::vector<const void*> mChannelBufferPointers;
    std::uint32_t mSampleRate;
    std::uint32_t mSampleCount;
    std::atomic<std::uint32_t> mSeekTarget;
    bool mPaused;
    bool mAtEnd;
};

inline const ma_data_source_vtable SoundSource::sVtable = ma_data_source_vtable{
    dataSourceRead,
    dataSourceSeek,
    dataSourceGetDataFormat,
    dataSourceGetCursor,
    dataSourceGetLength,
    nullptr,
    0,
};

} // namespace playback