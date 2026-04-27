#include "playback/source.hpp"
#include "decode/decoder.hpp"
#include "sound/sound.hpp"
#include "resource/opus.hpp"

#include <atomic>
#include <ranges>

namespace playback {

SoundSource::~SoundSource() {
    ma_device_stop(&mDevice);
    ma_data_source_uninit(&mBase);
    ma_device_uninit(&mDevice);
}

auto SoundSource::initialize(const sound::Sound& sound) -> bool {
    if (sound.getChannelCount() < 1) {
        return false;
    }

    for (const auto& channel : sound.getChannels()) {
        if (channel.getSampleCount() != sound.getSampleCount()) {
            return false;
        }

        auto decoder = decode::CreateStreamDecoder(
            sound.isPrefetch() && channel.getSampleFormat() == sound::Format::Opus ? sound::Format::PcmInt16 : channel.getSampleFormat()
        );
        if (!decoder->initialize(channel, sound.getEndian())) {
            return false;
        }
        mDecoders.emplace_back(std::move(decoder));
    }

    auto dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &sVtable;
    if (ma_data_source_init(&dataSourceConfig, &mBase) != MA_SUCCESS) {
        return false;
    }

    auto deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_s16;
    deviceConfig.playback.channels = sound.getChannelCount();
    deviceConfig.sampleRate        = sound.getSampleRate();
    deviceConfig.dataCallback      = dataCallback;
    deviceConfig.pUserData         = &mBase;
    if (ma_device_init(nullptr, &deviceConfig, &mDevice) != MA_SUCCESS) {
        return false;
    }

    mChannelBuffers.resize(mDecoders.size());
    mChannelBufferPointers.resize(mDecoders.size());
    for (auto& buffer : mChannelBuffers) {
        buffer.resize(resource::cOpusSamplesPerFrame * 4);
    }

    mSampleRate = sound.getSampleRate();
    mSampleCount = sound.getSampleCount();
    mSeekTarget.store(cInvalidSeekTarget, std::memory_order_relaxed);
    mPaused = true;
    mAtEnd = false;

    // technically each channel would be handled individually, but surely they wouldn't be different, right...?
    const auto& channel = sound.getChannels()[0];
    if (channel.getLoopEnd() != -1) {
        setRange(0, static_cast<std::uint32_t>(channel.getLoopEnd()));
        setLoop(true);
        const auto start = channel.getLoopSeekPointIndex() < channel.getSeekPointCount() ? channel.getSeekPoints()[channel.getLoopSeekPointIndex()] : 0;
        setLoopPoints(start, channel.getLoopEnd());
    }

    return true;
}

auto SoundSource::setLoop(bool isLooping) -> void {
    ma_data_source_set_looping(&mBase, isLooping);
}

auto SoundSource::setLoopPoints(std::uint32_t loopStart, std::uint32_t loopEnd) -> void {
    ma_data_source_set_loop_point_in_pcm_frames(&mBase, loopStart, loopEnd == cEndOfSamples ? mSampleCount : loopEnd);
}

auto SoundSource::setRange(std::uint32_t start, std::uint32_t end) -> void {
    ma_data_source_set_range_in_pcm_frames(&mBase, start, end == cEndOfSamples ? mSampleCount : end);
}

auto SoundSource::dataCallback(ma_device* pDevice, void* pOutput, const void* pInput [[maybe_unused]], ma_uint32 frameCount) -> void {
    ma_data_source* pDataSource = (ma_data_source*)pDevice->pUserData;
    if (pDataSource == nullptr) {
        return;
    }

    ma_data_source_read_pcm_frames(pDataSource, pOutput, frameCount, nullptr);
}

auto SoundSource::dataSourceRead(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) -> ma_result {
    auto dataSource = reinterpret_cast<SoundSource*>(pDataSource);

    const auto seekTarget = dataSource->mSeekTarget.load(std::memory_order_relaxed);
    if (seekTarget != 0xffff'ffff) {
        for (auto& decoder : dataSource->mDecoders) {
            decoder->seek(seekTarget);
        }
        dataSource->mSeekTarget.store(0xffff'ffff, std::memory_order_relaxed);
    }

    auto framesRead = 0ull;
    for (const auto i : std::views::iota(0ull, dataSource->mDecoders.size())) {
        auto& buffer = dataSource->mChannelBuffers[i];
        auto& decoder = dataSource->mDecoders[i];
        buffer.resize(frameCount);
        framesRead = decoder->read(buffer);
        dataSource->mChannelBufferPointers[i] = buffer.data();
    }

    ma_interleave_pcm_frames(ma_format_s16, dataSource->mChannelBuffers.size(), framesRead, dataSource->mChannelBufferPointers.data(), pFramesOut);

    if (pFramesRead) {
        *pFramesRead = framesRead;
    }

    if (dataSource->mDecoders.size() < 1) {
        dataSource->mAtEnd = true;
    } else {
        dataSource->mAtEnd = dataSource->mDecoders[0]->tell() >= dataSource->mDecoders[0]->getSampleCount();
    }

    return MA_SUCCESS;
}

auto SoundSource::dataSourceSeek(ma_data_source* pDataSource, ma_uint64 frameIndex) -> ma_result {
    auto dataSource = reinterpret_cast<SoundSource*>(pDataSource);
    for (auto& decoder : dataSource->mDecoders) {
        decoder->seek(frameIndex);
    }
    dataSource->mSeekTarget.store(cInvalidSeekTarget, std::memory_order_relaxed);
    return MA_SUCCESS;
}

auto SoundSource::dataSourceGetDataFormat(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap) -> ma_result {
    auto dataSource = reinterpret_cast<SoundSource*>(pDataSource);
    if (pFormat) {
        *pFormat = ma_format_s16;
    }
    if (pChannels) {
        *pChannels = dataSource->mDecoders.size();
    }
    if (pSampleRate) {
        *pSampleRate = dataSource->mSampleRate;
    }
    if (pChannelMap) {
        for (const auto& [index, decoder] : std::views::enumerate(std::as_const(dataSource->mDecoders))) {
            if (static_cast<size_t>(index) >= channelMapCap) {
                break;
            }
            switch (decoder->getOutputChannel()) {
                case sound::ChannelPan::Left:
                    pChannelMap[index] = MA_CHANNEL_FRONT_LEFT;
                    break;
                case sound::ChannelPan::Right:
                    pChannelMap[index] = MA_CHANNEL_FRONT_RIGHT;
                    break;
                case sound::ChannelPan::Center:
                    pChannelMap[index] = MA_CHANNEL_FRONT_CENTER;
                    break;
                case sound::ChannelPan::LowFrequencyEffect:
                    pChannelMap[index] = MA_CHANNEL_LFE;
                    break;
                case sound::ChannelPan::LeftSurround:
                    pChannelMap[index] = MA_CHANNEL_SIDE_LEFT;
                    break;
                case sound::ChannelPan::RightSurround:
                    pChannelMap[index] = MA_CHANNEL_SIDE_RIGHT;
                    break;
            }
        }
    }
    return MA_SUCCESS;
}

auto SoundSource::dataSourceGetCursor(ma_data_source* pDataSource, ma_uint64* pCursor) -> ma_result {
    const auto dataSource = reinterpret_cast<SoundSource*>(pDataSource);
    if (dataSource->mDecoders.size() < 1) {
        return MA_INVALID_DATA;
    }

    if (pCursor) {
        *pCursor = static_cast<ma_uint64>(dataSource->mDecoders[0]->tell());
    }
    return MA_SUCCESS;
}

auto SoundSource::dataSourceGetLength(ma_data_source* pDataSource, ma_uint64* pLength) -> ma_result {
    const auto dataSource = reinterpret_cast<SoundSource*>(pDataSource);
    if (dataSource->mDecoders.size() < 1) {
        return MA_INVALID_DATA;
    }

    if (pLength) {
        *pLength = static_cast<ma_uint64>(dataSource->mDecoders[0]->getSampleCount());
    }
    return MA_SUCCESS;
}

auto SoundSource::dataSourceOnSetLooping(ma_data_source* pDataSource [[maybe_unused]], ma_bool32 isLooping [[maybe_unused]]) -> ma_result {
    return MA_SUCCESS;
}

auto SoundSource::play() -> bool {
    if (mPaused) {
        mPaused = ma_device_start(&mDevice) != MA_SUCCESS;
    } else if (mAtEnd) {
        ma_device_stop(&mDevice);
        seek(0);
        mPaused = ma_device_start(&mDevice) != MA_SUCCESS; 
    }
    return !mPaused;
}

auto SoundSource::pause() -> bool {
    if (!mPaused) {
        mPaused = ma_device_stop(&mDevice) == MA_SUCCESS;
    }
    return mPaused;
}

auto SoundSource::stop() -> bool {
    mAtEnd = true;
    return ma_device_stop(&mDevice) == MA_SUCCESS;
}

auto SoundSource::seek(std::int32_t target, bool fromCurrent) -> std::uint32_t {
    if (fromCurrent) {
        const auto currTarget = mSeekTarget.load(std::memory_order_relaxed);
        if (currTarget != cInvalidSeekTarget) {
            target += currTarget;
        } else if (mDecoders.size() > 0) {
            target += mDecoders[0]->tell();
        }
    }
    if (target < 0) {
        target = 0;
    }
    mSeekTarget.store(static_cast<std::uint32_t>(target), std::memory_order_relaxed);
    return static_cast<std::uint32_t>(target);
}

auto SoundSource::tell() const -> std::uint32_t {
    const auto currTarget = mSeekTarget.load(std::memory_order_relaxed);
    if (currTarget != cInvalidSeekTarget) {
        return currTarget;
    }

    if (mDecoders.size() > 0) {
        return mDecoders[0]->tell();
    }

    return 0;
}

} // namespace playback