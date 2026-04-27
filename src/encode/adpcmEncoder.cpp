#include "encode/adpcmEncoder.hpp"
#include "sound/channel.hpp"

#include <dsptool.h>

#include <cstring>

namespace encode {

auto AdpcmEncoder::initialize(const sound::Channel& channel, std::endian endian [[maybe_unused]]) -> bool {
    mIsLoop = channel.getLoopEnd() != -1;
    return true;
}

auto AdpcmEncoder::encode(std::span<const std::int16_t> inBuf, std::vector<std::uint8_t>& outBuf) -> bool {
    const auto numSamples = static_cast<std::uint32_t>(inBuf.size());
    const auto reqBufSize = mIsLoop ? dsptool_getBytesForAdpcmSamples(numSamples) : dsptool_getBytesForAdpcmBuffer(numSamples);
    outBuf.resize(reqBufSize);

    auto ctx = ADPCMINFO{};
    // dsptool writes back the encoded pcm samples into the src buffer so we need to store this somewhere
    auto sampleBuf = std::vector<std::int16_t>( inBuf.begin(), inBuf.end() );
    dsptool_encode(sampleBuf.data(), outBuf.data(), &ctx, numSamples, mIsLoop);

    std::memcpy(&mParameter, ctx.coef, sizeof(ctx.coef));

    return true;
}

} // namespace encode