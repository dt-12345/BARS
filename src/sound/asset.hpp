#pragma once

#include "sound/channel.hpp"
#include "sound/metadata.hpp"
#include "sound/sound.hpp"

#include <string>
#include <string_view>

namespace sound {

class Archive;

class Asset {
public:
    Asset() = default;

    [[nodiscard]] auto getMetadata() -> Metadata& { return mMetadata; }
    [[nodiscard]] auto getMetadata() const -> const Metadata& { return mMetadata; }

    [[nodiscard]] auto getSound() -> Sound& { return mSound; }
    [[nodiscard]] auto getSound() const -> const Sound& { return mSound; }

    [[nodiscard]] auto getStreamInfo() -> StreamInfo& { return mStreamInfo; }
    [[nodiscard]] auto getStreamInfo() const -> const StreamInfo& { return mStreamInfo; }

    auto setName(std::string_view name) -> void { mMetadata.setName(name); }
    [[nodiscard]] auto getName() const -> std::string_view { return mMetadata.getName(); }

    auto setEndian(std::endian endian) -> void { mEndian = endian; }
    [[nodiscard]] auto getEndian() const -> std::endian { return mEndian; }

    // sets sound for this asset, recalculates metadata and sets stream info if prefetch required
    auto setSound(std::string_view name, Sound& sound, bool keepMeta) -> bool;

    auto dumpMetadata() const -> std::string { return mMetadata.dumpMetadata(); }

private:
    Metadata mMetadata;
    Sound mSound;
    StreamInfo mStreamInfo;
    std::endian mEndian;
};

} // namespace sound