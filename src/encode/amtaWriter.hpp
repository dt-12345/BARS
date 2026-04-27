#pragma once

#include "common/writer.hpp"
#include "sound/metadata.hpp"

#include <map>

namespace encode {
    
class AmtaWriter {
public:
    static auto Write(const sound::Metadata& meta, common::BinaryWriter& writer) -> bool;
    static auto Write(const sound::Metadata& meta, std::vector<std::uint8_t>& buffer) -> bool;
    static auto Write(const sound::Metadata& meta, std::string_view path) -> bool;

private:
    static auto WriteRelativeOffset(common::BinaryWriter& writer, std::uint32_t offsetValue, std::uint32_t offsetBase) -> void;
    static auto WriteOptData(const sound::OptionalMetadata& opts, common::BinaryWriter& writer) -> bool;
    static auto WriteMarker(const sound::Marker& marker, common::BinaryWriter& writer, std::map<std::uint32_t, std::string>& strings) -> bool;
};

} // namespace encode