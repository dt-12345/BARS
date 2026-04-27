#include "common/reader.hpp"
#include "decode/opusDecoder.hpp"
#include "opusDecoder.hpp"
#include "resource/opus.hpp"
#include "sound/channel.hpp"

#include <opus.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace decode {

class Impl {
public:
    Impl(std::uint32_t sampleRate, std::uint32_t numChannels) {
        const auto decoderSize = opus_decoder_get_size(numChannels);
        mDecoderMemory = std::make_unique<std::uint8_t[]>(decoderSize);

        auto decoder = getDecoder();
        if (opus_decoder_init(decoder, sampleRate, numChannels) != OPUS_OK) {
            mDecoderMemory.release();
        }
    }

    ~Impl()= default;

    [[nodiscard]] auto decode(const std::uint8_t* in, std::uint32_t inSize, std::int16_t* out, std::uint32_t outSampleCount) const -> std::int32_t {
        auto decoder = getDecoder();
        if (decoder == nullptr) {
            return OPUS_ALLOC_FAIL;
        }

        return opus_decode(decoder, in, inSize, out, outSampleCount, 0);
    }

    [[nodiscard]] auto getDecoder() const -> ::OpusDecoder* {
        return reinterpret_cast<::OpusDecoder*>(mDecoderMemory.get());
    }

    [[nodiscard]] auto getSamplesInPacket(const std::uint8_t* packet, std::uint32_t inSize) const -> std::uint32_t {
        auto decoder = getDecoder();
        if (decoder == nullptr) {
            return 0;
        }
        return opus_decoder_get_nb_samples(decoder, packet, inSize);
    }

private:
    std::unique_ptr<std::uint8_t[]> mDecoderMemory;
};

static auto DecodeFrame(Impl& impl, common::BinaryReader& reader, std::int16_t* samplesOut, std::uint32_t maxSamplesOut) -> std::int32_t {
    const auto encodedSize = reader.read<std::uint32_t>(std::endian::big);
    [[maybe_unused]] const auto finalRange = reader.read<std::uint32_t>(std::endian::big);

    if (reader.tell() + encodedSize > reader.size()) {
        return OPUS_BUFFER_TOO_SMALL;
    }

    const auto result = impl.decode(reader.data() + reader.tell(), encodedSize, samplesOut, maxSamplesOut);
    if (result >= 0) {
        reader.skip(encodedSize);
    }
    return result;
}

// because opus is lossy, some extra samples from the original data is stored to smooth the transition from prefetch to opus
static auto ApplyBlend(std::span<std::int16_t> samples, const std::span<std::int16_t> blendSamples, std::uint32_t startSample, std::uint32_t prefetchSampleCount) -> void {
    if (prefetchSampleCount < startSample || prefetchSampleCount >= startSample + samples.size()) {
        return;
    }

    if (blendSamples.size() < 1) {
        return;
    }

    auto i = 0ull;
    const auto max = blendSamples.size() - 1;
    for (auto remaining = blendSamples.size(); remaining != 0; --remaining, ++i) {
        auto sample = std::uint16_t(0);
        if (max != 0) {
            // interpolate between blend sample and opus, weighted towards whichever side the sample is closer to
            sample = (blendSamples[i] * (remaining - 1) + samples[(prefetchSampleCount - startSample) + i] * i) / max;
        }
        samples[(prefetchSampleCount - startSample) + i] = sample;
    }
}

auto OpusDecoder::initialize(const sound::Channel& channel [[maybe_unused]], std::endian endian) -> bool {
    mEndian = endian;
    return true;
}

// the game just hardcodes the frame size to 960, but we'll do this instead
auto OpusDecoder::getSampleAlignment(std::span<const std::uint8_t> inBuf) const -> std::uint32_t {
    if (inBuf.size() < sizeof(resource::OpusChunk) + sizeof(resource::OpusHeader)) {
        return 0;
    }
    
    auto reader = common::BinaryReader(inBuf, mEndian);
    const auto headerChunkType = reader.read<resource::ChunkType>();
    if (headerChunkType != resource::ChunkType::Header) {
        return 0;
    }

    const auto headerChunkSize = reader.read<std::uint32_t>();
    if (headerChunkSize != sizeof(resource::OpusHeader)) {
        return 0;
    }

    reader.skip(offsetof(resource::OpusHeader, sampleRate));
    const auto sampleRate = reader.read<std::uint32_t>();
    if (!resource::IsValidOpusSampleRate(sampleRate)) {
        return 0;
    }

    return sampleRate / (1000 / resource::cOpusFrameDuration);
}

auto OpusDecoder::decode(std::span<const std::uint8_t> inBuf, std::span<std::int16_t> outBuf) -> std::uint32_t {
    if (inBuf.size() < sizeof(resource::OpusChunk) + sizeof(resource::OpusHeader)) {
        return 0;
    }
    
    auto reader = common::BinaryReader(inBuf, mEndian);
    const auto headerChunkType = reader.read<resource::ChunkType>();
    if (headerChunkType != resource::ChunkType::Header) {
        return 0;
    }

    const auto headerChunkSize = reader.read<std::uint32_t>();
    if (headerChunkSize != sizeof(resource::OpusHeader)) {
        return 0;
    }

    const auto version = reader.read<std::uint8_t>();
    if (version != resource::cOpusHeaderVersion) {
        return 0;
    }

    const auto channelCount = reader.read<std::uint8_t>();
    if (channelCount != 1) {
        return 0;
    }

    reader.skip(2); // frame size
    const auto sampleRate = reader.read<std::uint32_t>();
    if (!resource::IsValidOpusSampleRate(sampleRate)) {
        return 0;
    }

    const auto dataOffset = reader.read<std::uint32_t>();
    reader.seek(dataOffset);

    const auto dataChunkType = reader.read<resource::ChunkType>();
    if (dataChunkType != resource::ChunkType::Data) {
        return 0;
    }

    const auto dataChunkSize = reader.read<std::uint32_t>();
    if (reader.tell() + dataChunkSize > reader.size()) { // we don't have enough data
        return 0;
    }

    auto impl = Impl(sampleRate, channelCount);
    auto remaining = static_cast<std::uint32_t>(outBuf.size());
    auto samplesRead = 0u;
    auto samplesOut = outBuf.data();
    while (remaining > 0) {
        const auto result = DecodeFrame(impl, reader, samplesOut, remaining);
        if (result < 0) {
            return samplesRead; // decoding failed
        }
        samplesRead += result;
        remaining -= result;
        samplesOut += result;
    }

    return samplesRead;
}

struct OpusStreamDecoder::DecoderState {
    std::vector<std::int16_t> lastFrame;
    size_t lastFrameRemainingSamples;
    std::unique_ptr<Impl> impl;
    std::vector<std::int16_t> blendSamples;
    std::uint32_t prefetchSampleCount;
    std::uint32_t delayCompensation;
    std::uint32_t sampleRate;
    std::uint32_t frameSize;
    std::uint32_t samplesRead;
    std::uint32_t totalSampleCount;

    auto getDecoder() const -> ::OpusDecoder* {
        return impl->getDecoder();
    }
};

OpusStreamDecoder::~OpusStreamDecoder() {
    if (mState) {
        delete mState;
    }
}

auto OpusStreamDecoder::initialize(const sound::Channel& channel, std::endian endian) -> bool {
    if (channel.getRawSampleData().size() < sizeof(resource::OpusChunk) + sizeof(resource::OpusHeader)) {
        return false;
    }
    
    auto reader = common::BinaryReader(channel.getRawSampleData(), endian);
    if (reader.read<resource::ChunkType>() != resource::ChunkType::Header) {
        return false;
    }

    if (reader.read<std::uint32_t>() != sizeof(resource::OpusHeader)) {
        return false;
    }

    if (reader.read<std::uint8_t>() != resource::cOpusHeaderVersion) {
        return false;
    }

    // only 1 channel supported
    if (reader.read<std::uint8_t>() != 1) {
        return false;
    }

    reader.skip(2); // frame size

    mState = new DecoderState();
    mState->sampleRate = reader.read<std::uint32_t>();
    if (!resource::IsValidOpusSampleRate(mState->sampleRate)) {
        return false;
    }
    mState->frameSize = mState->sampleRate / (1000 / resource::cOpusFrameDuration);
    const auto dataOffset = reader.read<std::uint32_t>();

    reader.skip(8);
    mState->delayCompensation = static_cast<std::uint32_t>(reader.read<std::int16_t>());
    
    reader.seek(dataOffset);
    if (reader.read<resource::ChunkType>() != resource::ChunkType::Data) {
        return false;
    }

    const auto dataSize = reader.read<std::uint32_t>();
    if (reader.tell() + dataSize > reader.size()) {
        return false;
    }

    mOutputChannel = channel.getOutputChannel();
    mData = { reader.data() + reader.tell(), dataSize };
    mOffset = 0;
    mState->lastFrame.resize(mState->frameSize);
    mState->lastFrameRemainingSamples = 0;
    mState->impl = std::make_unique<Impl>(mState->sampleRate, 1);
    if (mState->getDecoder() == nullptr) {
        return false;
    }

    while (reader.tell() < reader.size()) {
        const auto packetSize = reader.read<std::uint32_t>(std::endian::big);
        reader.skip(4); // final range
        mState->totalSampleCount += mState->impl->getSamplesInPacket(reader.data() + reader.tell(), packetSize);
        reader.skip(packetSize);
    }

    if (channel.getOpusBlendSampleCount() > 0) {
        mState->blendSamples.assign(channel.getOpusBlendSamples().begin(), channel.getOpusBlendSamples().end());
        mState->prefetchSampleCount = channel.getPrefetchSampleCount();
    } else {
        mState->blendSamples.clear();
        mState->prefetchSampleCount = 0;
    }

    return true;
}

auto OpusStreamDecoder::copyFrame(std::span<std::int16_t> outBuf) -> std::uint32_t {
    // if (mState->samplesRead < mState->delayCompensation) {
    //     const auto samplesToSkip = mState->delayCompensation - mState->samplesRead;
    //     if (mState->lastFrameRemainingSamples >= samplesToSkip) {
    //         mState->lastFrameRemainingSamples -= samplesToSkip;
    //     } else {
    //         mState->lastFrameRemainingSamples = 0;
    //     }
    //     mState->samplesRead += samplesToSkip;
    // }

    if (mState->lastFrameRemainingSamples < 1) {
        return 0;
    }

    const auto samplesToRead = std::min(outBuf.size(), mState->lastFrameRemainingSamples);
    std::memcpy(outBuf.data(), mState->lastFrame.data() + mState->frameSize - mState->lastFrameRemainingSamples, samplesToRead * sizeof(std::int16_t));
    mState->lastFrameRemainingSamples -= samplesToRead;
    mState->samplesRead += samplesToRead;
    return samplesToRead;
}

auto OpusStreamDecoder::read(std::span<std::int16_t> outBuf) -> std::uint32_t {
    auto remaining = outBuf.size();
    auto samplesRead = 0u;
    auto samplesOut = outBuf.data();

    const auto samplesFromLastFrame = copyFrame(outBuf);
    samplesOut += samplesFromLastFrame;
    samplesRead += samplesFromLastFrame;
    remaining -= samplesFromLastFrame;
    if (remaining < 1) {
        return samplesRead;
    }

    if (mOffset >= mData.size()) {
        return samplesRead;
    }

    while (remaining > 0) {
        auto reader = common::BinaryReader({ mData.data() + mOffset, mData.size() - mOffset });
        const auto result = DecodeFrame(*mState->impl.get(), reader, mState->lastFrame.data(), mState->lastFrame.size());
        if (result < 0) {
            return samplesRead; // decoding failed
        }
        ApplyBlend(mState->lastFrame, mState->blendSamples, mState->samplesRead, 0);
        mOffset += reader.tell();
        mState->lastFrameRemainingSamples = result;
        const auto samplesReadFromFrame = copyFrame({ samplesOut, remaining });
        samplesOut += samplesReadFromFrame;
        samplesRead += samplesReadFromFrame;
        remaining -= samplesReadFromFrame;
    }

    return samplesRead;
}

auto OpusStreamDecoder::seek(std::uint32_t samplePos) -> void {
    if (samplePos == tell()) {
        return;
    }

    if (samplePos >= getSampleCount()) {
        samplePos = getSampleCount();
    }

    if (samplePos > tell()) {
        auto remaining = samplePos - tell();
        if (remaining <= mState->lastFrameRemainingSamples) {
            mState->lastFrameRemainingSamples -= remaining;
            return;
        }
        mState->lastFrameRemainingSamples = 0;
        remaining -= mState->lastFrameRemainingSamples;
        while (remaining > 0) {
            auto reader = common::BinaryReader({ mData.data() + mOffset, mData.size() - mOffset });
            const auto packetSize = reader.read<std::uint32_t>(std::endian::big);
            reader.skip(4); // final range
            const auto numSamples = mState->impl->getSamplesInPacket(reader.data() + reader.tell(), packetSize);
            mOffset += packetSize + sizeof(resource::OpusPacket);
            if (remaining > numSamples) {
                remaining -= numSamples;
                mState->samplesRead += numSamples;
                reader.skip(packetSize);
            } else {
                reader.rewind(8);
                const auto result = DecodeFrame(*mState->impl.get(), reader, mState->lastFrame.data(), mState->lastFrame.size());
                if (result < 0) {
                    return;
                }
                mState->lastFrameRemainingSamples = result - remaining;
                mState->samplesRead += result - remaining;
                return;
            }
        }
    } else if (samplePos < tell()) {
        mOffset = 0;
        mState->lastFrameRemainingSamples = 0;
        mState->samplesRead = 0;
        auto reader = common::BinaryReader({ mData.data() + mOffset, mData.size() - mOffset });
        while (samplePos > 0) {
            const auto packetSize = reader.read<std::uint32_t>(std::endian::big);
            reader.skip(4); // final range
            const auto numSamples = mState->impl->getSamplesInPacket(reader.data() + reader.tell(), packetSize);
            mOffset += packetSize + sizeof(resource::OpusPacket);
            if (samplePos > numSamples) {
                samplePos -= numSamples;
                mState->samplesRead += numSamples;
                reader.skip(packetSize);
            } else {
                reader.rewind(8);
                const auto result = DecodeFrame(*mState->impl.get(), reader, mState->lastFrame.data(), mState->lastFrame.size());
                if (result < 0) {
                    return;
                }
                mState->lastFrameRemainingSamples = result - samplePos;
                mState->samplesRead += result - samplePos;
                return;
            }
        }
    }
}

auto OpusStreamDecoder::tell() const -> std::uint32_t {
    // return mState->samplesRead < mState->delayCompensation ? 0 : mState->samplesRead - mState->delayCompensation;
    return mState->samplesRead;
}

auto OpusStreamDecoder::getSampleCount() const -> std::uint32_t {
    // return mState->totalSampleCount < mState->delayCompensation ? 0 : mState->totalSampleCount - mState->delayCompensation;
    return mState->totalSampleCount;
}

} // namespace decode