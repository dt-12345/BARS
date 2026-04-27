#pragma once

#include "sound/types.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace sound {
class Channel;
} // namespace sound

namespace encode {

class IEncoder {
public:
    virtual ~IEncoder() = default;
    [[nodiscard]] virtual auto initialize(const sound::Channel& channel, std::endian endian) -> bool = 0;
    // pcm16 input -> any
    [[nodiscard]] virtual auto encode(std::span<const std::int16_t> inBuf, std::vector<std::uint8_t>& outBuf) -> bool = 0;
};

auto CreateEncoder(sound::Format fmt) -> std::unique_ptr<IEncoder>;

} // namespace encode