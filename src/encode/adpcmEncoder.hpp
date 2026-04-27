#pragma once

#include "resource/adpcm.hpp"

#include "encode/encoder.hpp"

namespace encode {

class AdpcmEncoder : public IEncoder {
public:
    AdpcmEncoder() = default;

    ~AdpcmEncoder() override = default;
    [[nodiscard]] auto initialize(const sound::Channel& channel, std::endian endian) -> bool override;
    [[nodiscard]] auto encode(std::span<const std::int16_t> inBuf, std::vector<std::uint8_t>& outBuf) -> bool override;

    auto getParameter() const -> const resource::AdpcmParameter& { return mParameter; }

private:
    resource::AdpcmParameter mParameter;
    bool mIsLoop;
};

} // namespace encode