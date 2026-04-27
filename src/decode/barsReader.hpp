#pragma once

#include "common/reader.hpp"
#include "decode/decodeResult.hpp"
#include "sound/archive.hpp"

#include <expected>
#include <memory>
#include <span>

namespace resource {
struct ResAssetOffset;
} // namespace resource

namespace decode {

class BarsReader {
public:
    [[nodiscard]] static auto Read(common::BinaryReader& reader, sound::Archive& archive) -> Result;
    [[nodiscard]] static auto Read(common::BinaryReader& reader) -> std::expected<std::unique_ptr<sound::Archive>, Result>;
    [[nodiscard]] static auto Read(std::span<const std::uint8_t> data) -> std::expected<std::unique_ptr<sound::Archive>, Result>;
    [[nodiscard]] static auto Read(std::string_view path) -> std::expected<std::unique_ptr<sound::Archive>, Result>;

private:
    [[nodiscard]] static auto ReadAsset(common::BinaryReader& reader, sound::Asset& asset, const resource::ResAssetOffset& offset) -> Result;
};

} // namespace decode