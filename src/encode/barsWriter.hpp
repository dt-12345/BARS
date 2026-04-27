#pragma once

#include "common/writer.hpp"
#include "sound/archive.hpp"

namespace encode {
    
class BarsWriter {
public:
    static auto Write(const sound::Archive& archive, common::BinaryWriter& writer) -> bool;
    static auto Write(const sound::Archive& archive, std::vector<std::uint8_t>& buffer) -> bool;
    static auto Write(const sound::Archive& archive, std::string_view path) -> bool;
};

} // namespace encode