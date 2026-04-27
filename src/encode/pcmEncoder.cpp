#include "common/endian.hpp"
#include "encode/pcmEncoder.hpp"

#include <algorithm>
#include <cstring>

namespace encode {

auto PcmEncoder::initialize(const sound::Channel& channel [[maybe_unused]], std::endian endian) -> bool {
    mEndian = endian;
    return true;
}

auto PcmEncoder::encode(std::span<const std::int16_t> inBuf, std::vector<std::uint8_t>& outBuf) -> bool {
    outBuf.resize(inBuf.size_bytes());
    if (mEndian == std::endian::native) {
        std::memcpy(outBuf.data(), inBuf.data(), inBuf.size() * sizeof(std::int16_t));
    } else {
        auto range = std::span{ reinterpret_cast<std::int16_t*>(outBuf.data()), inBuf.size() };
        std::transform(inBuf.cbegin(), inBuf.cend(), range.begin(), common::ByteSwap<std::int16_t>);
    }
    return true;
}

} // namespace encode