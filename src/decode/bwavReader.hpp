#pragma once

#include "common/reader.hpp"
#include "decode/decodeResult.hpp"
#include "sound/sound.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>

namespace decode {

class BwavReader {
public:
    [[nodiscard]] static auto Read(common::BinaryReader& reader, sound::Sound& sound, sound::StreamInfo* streamInfo = nullptr) -> Result;
    [[nodiscard]] static auto Read(common::BinaryReader& reader, sound::StreamInfo* streamInfo = nullptr) -> std::expected<std::unique_ptr<sound::Sound>, Result>;
    [[nodiscard]] static auto Read(std::span<const std::uint8_t> data, sound::StreamInfo* streamInfo = nullptr) -> std::expected<std::unique_ptr<sound::Sound>, Result>;
    [[nodiscard]] static auto Read(std::string_view path, sound::StreamInfo* streamInfo = nullptr) -> std::expected<std::unique_ptr<sound::Sound>, Result>;

private:
    [[nodiscard]] static auto ReadChannel(common::BinaryReader& reader, sound::Channel& channel, size_t baseOffset, bool isPrefetch, sound::StreamInfo* streamInfo) -> Result;

};

} // namespace decode