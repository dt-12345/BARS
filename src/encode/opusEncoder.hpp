#pragma once

#include "encode/encoder.hpp"

namespace encode {

class OpusEncoder : public IEncoder {
public:
    OpusEncoder() = default;

    ~OpusEncoder() override = default;
    [[nodiscard]] auto initialize(const sound::Channel& channel, std::endian endian) -> bool override;
    [[nodiscard]] auto encode(std::span<const std::int16_t> inBuf, std::vector<std::uint8_t>& outBuf) -> bool override;

private:
    std::uint32_t mSampleRate;
    std::endian mEndian;
};

} // namespace encode