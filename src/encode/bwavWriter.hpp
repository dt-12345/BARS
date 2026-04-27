#pragma once

#include "common/writer.hpp"
#include "sound/sound.hpp"

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace resource {
struct AdpcmContext;
} // namespace resource

namespace encode {
    
class BwavWriter {
public:
    static auto WritePrefetch(const sound::Sound& sound, const sound::StreamInfo& streamInfo, common::BinaryWriter& writer) -> bool;
    static auto WritePrefetch(const sound::Sound& sound, const sound::StreamInfo& streamInfo, std::vector<std::uint8_t>& buffer) -> bool;
    static auto Write(const sound::Sound& sound, common::BinaryWriter& writer, sound::StreamInfo* streamInfo = nullptr) -> bool;
    static auto Write(const sound::Sound& sound, std::vector<std::uint8_t>& buffer, sound::StreamInfo* streamInfo = nullptr) -> bool;
    static auto Write(const sound::Sound& sound, std::string_view path, sound::StreamInfo* streamInfo = nullptr) -> bool;

private:
    static auto WriteChannel(const sound::Channel& channel, common::BinaryWriter& writer, size_t& dataOffset, sound::StreamInfo* streamInfo) -> bool;
    static auto WriteChannelPrefetch(const sound::Channel& channel, common::BinaryWriter& writer, size_t& dataOffset,
                                std::uint32_t sampleOffset, std::uint32_t sampleCount, const std::unordered_map<std::uint32_t, resource::AdpcmContext>& seekPoints) -> bool;
};

} // namespace encode