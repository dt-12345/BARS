#include "common/writer.hpp"
#include "encode/opusEncoder.hpp"
#include "opus_defines.h"
#include "resource/opus.hpp"
#include "sound/channel.hpp"

#include <opus.h>

#include <array>

namespace encode {

class Impl {
public:
    Impl(std::uint32_t sampleRate, std::uint32_t numChannels) {
        const auto encoderSize = opus_encoder_get_size(numChannels);
        mEncoderMemory = std::make_unique<std::uint8_t[]>(encoderSize);

        auto encoder = getEncoder();
        if (opus_encoder_init(encoder, sampleRate, numChannels, OPUS_APPLICATION_AUDIO) != OPUS_OK) {
            mEncoderMemory.release();
        }
    }

    [[nodiscard]] auto encode(const std::int16_t* in, std::uint32_t inSize, std::uint8_t* out, std::uint32_t outSize) -> std::int32_t {
        auto encoder = getEncoder();
        if (encoder == nullptr) {
            return 0;
        }

        if (inSize != resource::cOpusSamplesPerFrame) {
            return OPUS_BAD_ARG;
        }

        return opus_encode(encoder, in, resource::cOpusSamplesPerFrame, out, outSize);
    }

    [[nodiscard]] auto getEncoder() const -> ::OpusEncoder* {
        return reinterpret_cast<::OpusEncoder*>(mEncoderMemory.get());
    }

    [[nodiscard]] auto getDelay() const -> std::uint16_t {
        auto encoder = getEncoder();
        if (encoder == nullptr) {
            return 0;
        }
        std::int32_t delay;
        opus_encoder_ctl(encoder, OPUS_GET_LOOKAHEAD(&delay));
        return static_cast<std::uint16_t>(delay);
    }

    [[nodiscard]] auto getFinalRange() const -> std::uint32_t {
        auto encoder = getEncoder();
        if (encoder == nullptr) {
            return 0;
        }
        std::uint32_t finalRange;
        opus_encoder_ctl(encoder, OPUS_GET_FINAL_RANGE(&finalRange));
        return finalRange;
    }

    static constexpr const auto cMaxBytesPerFrame = std::uint32_t(240);

    [[nodiscard]] auto getStagingBuffer() -> std::array<std::int16_t, resource::cOpusSamplesPerFrame>& {
        return mSampleBuffer;
    }

private:
    std::unique_ptr<std::uint8_t[]> mEncoderMemory;
    std::array<std::int16_t, resource::cOpusSamplesPerFrame> mSampleBuffer;
};

static auto EncodeFrame(Impl& impl, common::BinaryWriter& writer, const std::int16_t* samplesIn, std::uint32_t sampleCount) -> std::int32_t {
    auto inBuf = samplesIn;
    if (sampleCount < resource::cOpusSamplesPerFrame) {
        auto stagingBuffer = impl.getStagingBuffer().data();
        std::memcpy(stagingBuffer, samplesIn, sampleCount * sizeof(std::int16_t));
        std::memset(stagingBuffer + sampleCount, 0, (resource::cOpusSamplesPerFrame - sampleCount) * sizeof(std::int16_t));
        inBuf = stagingBuffer;
    }

    const auto sizeOffset = writer.tell();
    writer.skip(sizeof(resource::OpusPacket) + Impl::cMaxBytesPerFrame); // allocates required size
    writer.seek(sizeOffset + sizeof(resource::OpusPacket));
    const auto result = impl.encode(inBuf, resource::cOpusSamplesPerFrame, reinterpret_cast<std::uint8_t*>(writer.data() + writer.tell()), Impl::cMaxBytesPerFrame);
    if (result < 0) {
        return result;
    }
    writer.seek(sizeOffset);
    writer.write(static_cast<std::uint32_t>(result), std::endian::big);
    writer.write(impl.getFinalRange(), std::endian::big);
    writer.skip(result);
    return sizeof(resource::OpusPacket) + result;
}

auto OpusEncoder::initialize(const sound::Channel& channel, std::endian endian) -> bool {
    mSampleRate = channel.getSampleRate();
    mEndian = endian;
    return resource::IsValidOpusSampleRate(mSampleRate);
}

auto OpusEncoder::encode(std::span<const std::int16_t> inBuf, std::vector<std::uint8_t>& outBuf) -> bool {
    auto impl = Impl(mSampleRate, 1);

    auto writer = common::BinaryWriter();
    writer.setEndian(mEndian);
    writer.write(resource::ChunkType::Header);
    writer.write(static_cast<std::uint32_t>(sizeof(resource::OpusHeader)));
    writer.write(resource::cOpusHeaderVersion);
    writer.write(static_cast<std::uint8_t>(1)); // channels
    writer.write(static_cast<std::uint16_t>(0)); // frame size
    writer.write(mSampleRate);
    writer.write(static_cast<std::uint32_t>(sizeof(resource::OpusChunk) + sizeof(resource::OpusHeader))); // data chunk offset
    writer.write(0u); // unknown section offset
    writer.write(0u); // context offset
    writer.write(impl.getDelay());
    writer.write(static_cast<std::uint16_t>(0)); // padding

    writer.write(resource::ChunkType::Data);
    const auto dataSizeOffset = writer.tell();
    writer.write(0u); // fill in size later

    const auto baseOffset = writer.tell();
    auto dataSize = 0u;
    for (std::uint32_t sampleOffset = 0; sampleOffset < inBuf.size(); sampleOffset += resource::cOpusSamplesPerFrame) {
        const auto result = EncodeFrame(impl, writer, inBuf.data() + sampleOffset, std::min(resource::cOpusSamplesPerFrame, static_cast<std::uint32_t>(inBuf.size() - sampleOffset)));
        if (result < 0) {
            return false;
        }
        dataSize += result;
    }
    writer.writeAt(dataSize, dataSizeOffset);
    
    writer.buffer().erase(writer.buffer().begin() + baseOffset + dataSize, writer.buffer().end());
    outBuf.swap(writer.buffer());
    return true;
}
    
} // namespace encode