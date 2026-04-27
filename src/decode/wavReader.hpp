#pragma once

#include "decode/decodeResult.hpp"
#include "sound/sound.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>

namespace common {
class BinaryReader;
} // namespace common

namespace sound {
class Channel;
class Sound;
} // namespace sound

namespace decode {

class WavReader {
public:
    [[nodiscard]] static auto Read(common::BinaryReader& reader, sound::Sound& sound) -> Result;
    [[nodiscard]] static auto Read(common::BinaryReader& reader) -> std::expected<std::unique_ptr<sound::Sound>, Result>;
    [[nodiscard]] static auto Read(std::span<const std::uint8_t> data) -> std::expected<std::unique_ptr<sound::Sound>, Result>;
    [[nodiscard]] static auto Read(std::string_view path) -> std::expected<std::unique_ptr<sound::Sound>, Result>;
};

} // namespace decode