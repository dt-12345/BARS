#pragma once

#include "sound/types.hpp"

#include <cstdint>
#include <memory>
#include <span>

namespace sound {
class Channel;
} // namespace sound

namespace decode {

class IDecoder {
public:
    virtual ~IDecoder() = default;
    [[nodiscard]] virtual auto initialize(const sound::Channel& channel, std::endian endian) -> bool = 0;
    // any -> pcm16 output
    [[nodiscard]] virtual auto decode(std::span<const std::uint8_t> inBuf, std::span<std::int16_t> outBuf) -> std::uint32_t = 0;
    [[nodiscard]] virtual auto getSampleAlignment(std::span<const std::uint8_t> inBuf [[maybe_unused]]) const -> std::uint32_t { return 1; }
};

class IStreamDecoder {
public:
    virtual ~IStreamDecoder() = default;
    [[nodiscard]] virtual auto initialize(const sound::Channel& channel, std::endian endian) -> bool = 0;
    [[nodiscard]] virtual auto read(std::span<std::int16_t> outBuf) -> std::uint32_t = 0;
    virtual auto seek(std::uint32_t samplePos) -> void = 0;
    [[nodiscard]] virtual auto tell() const -> std::uint32_t = 0;
    [[nodiscard]] virtual auto getSampleCount() const -> std::uint32_t = 0;
    [[nodiscard]] virtual auto getOutputChannel() const -> sound::ChannelPan = 0;
};

auto CreateDecoder(sound::Format fmt) -> std::unique_ptr<IDecoder>;
auto CreateStreamDecoder(sound::Format fmt) -> std::unique_ptr<IStreamDecoder>;

} // namespace decode