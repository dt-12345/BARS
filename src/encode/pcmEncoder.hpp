#pragma once

#include "encode/encoder.hpp"

namespace encode {

class PcmEncoder : public IEncoder {
public:
    PcmEncoder() = default;

    ~PcmEncoder() override = default;
    [[nodiscard]] auto initialize(const sound::Channel& channel, std::endian endian) -> bool override;
    [[nodiscard]] auto encode(std::span<const std::int16_t> inBuf, std::vector<std::uint8_t>& outBuf) -> bool override;

private:
    std::endian mEndian;
};

} // namespace encode