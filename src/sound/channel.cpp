#include "common/align.hpp"
#include "decode/decoder.hpp"
#include "encode/adpcmEncoder.hpp"
#include "encode/encoder.hpp"
#include "sound/channel.hpp"
#include "resource/adpcm.hpp"
#include "resource/opus.hpp"

#include <soxr.h>

#include <cmath>
#include <thread>

namespace sound {

Channel::Channel() : 
    mSamples(),
    mSampleCount(0),
    mSampleRate(48000),
    mSampleFormat(Format::PcmInt16),
    mOutputChannel(ChannelPan::Center),
    mLoopEnd(-1),
    mLoopSeekPointIndex(0),
    mSeekPoints(),
    mOpusBlendSamples(),
    mAdpcmParameter() {}

auto Channel::setLoopInfo(std::vector<std::uint32_t>& seekPoints, std::uint16_t loopSeekPointIndex, std::int32_t loopEnd) -> void {
    mSeekPoints.swap(seekPoints);
    mLoopSeekPointIndex = loopSeekPointIndex;
    mLoopEnd = loopEnd;
}

auto Channel::createPrefetch(Channel& prefetch, std::endian endian) const -> bool {
    prefetch.mSampleRate = mSampleRate;
    prefetch.mSampleFormat = mSampleFormat;
    prefetch.mOutputChannel = mOutputChannel;
    prefetch.mLoopEnd = mLoopEnd;
    prefetch.mLoopSeekPointIndex = mLoopSeekPointIndex;
    prefetch.mSeekPoints.assign(mSeekPoints.begin(), mSeekPoints.end());
    prefetch.mAdpcmParameter = mAdpcmParameter;
    
    // generate samples
    const auto prefetchBufferSize = mSampleFormat == Format::Opus ? 0x12000ull : 0x2000ull;
    constexpr const auto blendSampleCount = 240u;
    prefetch.mSamples.clear();
    prefetch.mOpusBlendSamples.clear();
    switch (mSampleFormat) {
        case Format::PcmInt16: {
            if (mSamples.size() < prefetchBufferSize) {
                return false;
            }
            prefetch.mSamples.assign(mSamples.begin(), mSamples.begin() + prefetchBufferSize);
            prefetch.mSampleCount = prefetchBufferSize / sizeof(std::int16_t);
            break;
        }
        case Format::DspAdpcm: {
            if (mSamples.size() < prefetchBufferSize) {
                return false;
            }
            prefetch.mSamples.assign(mSamples.begin(), mSamples.begin() + prefetchBufferSize);
            prefetch.mSampleCount = resource::CalcAdpcmSampleCountFromByteSize(prefetchBufferSize);
            break;
        }
        case Format::Opus: {
            const auto sampleCount = static_cast<std::uint32_t>(prefetchBufferSize / sizeof(std::int16_t));
            if (mSampleCount < sampleCount + blendSampleCount) {
                return false;
            }
            prefetch.mSamples.resize(sampleCount * sizeof(std::int16_t));
            auto decoder = decode::CreateStreamDecoder(Format::Opus);
            if (!decoder->initialize(*this, endian)) {
                prefetch.mSamples.clear();
                prefetch.mSampleCount = 0;
                return false;
            }
            if (decoder->read({ reinterpret_cast<std::int16_t*>(prefetch.mSamples.data()), sampleCount }) != sampleCount) {
                prefetch.mSamples.clear();
                prefetch.mSampleCount = 0;
                return false;
            }
            prefetch.mSampleCount = sampleCount;
            prefetch.mOpusBlendSamples.resize(blendSampleCount);
            if (decoder->read({ prefetch.mOpusBlendSamples.data(), prefetch.mOpusBlendSamples.size() }) != blendSampleCount) {
                prefetch.mOpusBlendSamples.clear();
                return false;
            }
            break;
        }
    }

    return true;
}

auto Channel::setPrefetchInfo(const Channel& prefetch) -> void {
    mPrefetchSampleCount = prefetch.mSampleCount;
    mOpusBlendSamples.assign(prefetch.mOpusBlendSamples.begin(), prefetch.mOpusBlendSamples.end());
}

auto Channel::resampleAndConvert(Format fmt, std::uint32_t sampleRate, std::uint32_t sampleCount, std::endian endian) -> bool {
    if (mSampleFormat == fmt && sampleRate == mSampleRate && sampleCount == mSampleCount) {
        return true;
    }

    if (fmt == Format::Opus) {
        if (!resource::IsValidOpusSampleRate(sampleRate)) {
            return false;
        }
    }

    auto decoder = decode::CreateDecoder(mSampleFormat);
    if (!decoder) {
        return false;
    }

    if (!decoder->initialize(*this, endian)) {
        return false;
    }

    const auto baseSampleCount = mSampleFormat == Format::Opus ? common::AlignUp(mSampleCount, resource::cOpusSamplesPerFrame) : mSampleCount;
    auto pcmSamples = std::vector<std::int16_t>(baseSampleCount);

    if (decoder->decode(mSamples, pcmSamples) != baseSampleCount) {
        return false;
    }

    const auto outputRate = static_cast<double>(sampleRate) / static_cast<double>(mSampleRate);
    auto newSampleCount = sampleCount;
    if (mSampleRate != sampleRate) {
        newSampleCount = static_cast<std::uint32_t>(std::ceil(static_cast<double>(sampleCount) * outputRate));
        // if you use split channels, soxr expects the input + output to be an array of pointers to the channels
        const auto ioSpec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
        const auto qualSpec = soxr_quality_spec(SOXR_VHQ, 0);
        const auto rtSpc = soxr_runtime_spec(std::thread::hardware_concurrency());
        auto resampled = std::vector<std::int16_t>(newSampleCount);
        size_t idone, odone;
        if (soxr_oneshot(
            1.0, outputRate, 1,
            pcmSamples.data(), pcmSamples.size(), &idone,
            resampled.data(), resampled.size(), &odone,
            &ioSpec, &qualSpec, &rtSpc
        ) != 0) {
            return false;
        }
        pcmSamples.swap(resampled);
    }

    if (sampleCount > mSampleCount) {
        pcmSamples.resize(sampleCount);
    } else if (sampleCount < mSampleCount) {
        pcmSamples.erase(pcmSamples.begin() + sampleCount, pcmSamples.end());
    }

    auto encoder = encode::CreateEncoder(fmt);
    if (!encoder) {
        return false;
    }

    if (!encoder->initialize(*this, endian)) {
        return false;
    }

    auto samples = std::vector<std::uint8_t>{};
    if (!encoder->encode(pcmSamples, samples)) {
        return false;
    }

    mSamples.swap(samples);

    if (fmt == Format::Opus) {
        constexpr const auto prefetchSamples = 0x12000u / static_cast<std::uint32_t>(sizeof(std::int16_t));
        constexpr const auto blendSamples = 240u;
        if (sampleCount >= prefetchSamples + blendSamples) {
            mOpusBlendSamples.assign(pcmSamples.begin() + prefetchSamples, pcmSamples.begin() + prefetchSamples + blendSamples);
        }
    } else if (fmt == Format::DspAdpcm) {
        const auto adpcmEncoder = static_cast<const encode::AdpcmEncoder*>(encoder.get());
        mAdpcmParameter = adpcmEncoder->getParameter();
    }

    if (mSampleRate != sampleRate) {
        for (auto& point : mSeekPoints) {
            point = static_cast<std::uint32_t>(static_cast<double>(point) * outputRate);
        }

        if (mLoopEnd != -1) {
            mLoopEnd = static_cast<std::uint32_t>(static_cast<double>(mLoopEnd) * outputRate);
        }
        
        mSampleRate = sampleRate;
    }

    if (mSampleCount != newSampleCount) {
        std::erase_if(mSeekPoints, [=](const auto& point){ return point > newSampleCount; });
        if (mLoopEnd != -1 && static_cast<std::uint32_t>(mLoopEnd) > newSampleCount) {
            mLoopEnd = newSampleCount;
        }
        mSampleCount = newSampleCount;
    }
    
    mSampleFormat = fmt;
    return true;
}

auto Channel::requiresStreaming() const -> bool {
    return (mSampleFormat == Format::Opus) // the game always treats opus as having prefetch
        || (mSampleFormat == Format::DspAdpcm && mSampleCount > resource::CalcAdpcmSampleCountFromByteSize(0x2000))
        || (mSampleFormat == Format::PcmInt16 && mSampleCount > 0x2000 / sizeof(std::int16_t));
}

auto Channel::setLoopPoint(std::uint32_t loopPoint) -> void {
    if (mSeekPoints.empty()) {
        mSeekPoints.emplace_back(mLoopEnd != -1 ? mLoopEnd : mSampleCount);
    }
    
    if (mSeekPoints.size() == 1) {
        mSeekPoints.emplace_back(loopPoint);
        mLoopSeekPointIndex = 1;
    } else {
        mSeekPoints.erase(mSeekPoints.begin() + 1);
        mSeekPoints.emplace_back(loopPoint);
        mLoopSeekPointIndex = 1;
    }

    if (mLoopEnd == -1) {
        mLoopEnd = mSampleCount;
    }
}

} // namespace sound