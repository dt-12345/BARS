#pragma once

#include "common/reader.hpp"
#include "decode/decodeResult.hpp"
#include "sound/metadata.hpp"

namespace decode {

class AmtaReader {
public:
    [[nodiscard]] static auto Read(common::BinaryReader& reader, sound::Metadata& meta) -> Result;

private:
    [[nodiscard]] static auto ReadTrack(common::BinaryReader& reader, sound::Track& track) -> Result;
    [[nodiscard]] static auto ReadOptData(common::BinaryReader& reader, sound::OptionalMetadata& opt) -> Result;
};

} // namespace decode