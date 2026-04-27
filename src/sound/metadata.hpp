#pragma once

#include "sound/music.hpp"
#include "sound/sound.hpp"
#include "sound/types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <string>

namespace sound {

constexpr const auto cMaxTracks = 8u;
constexpr const auto cMaxChannelsPerTrack = 6u;

class Track {
public:
    Track() = default;
    
    auto addChannel(std::uint8_t inputIndex, ChannelPan outputPan) -> bool;

    [[nodiscard]] auto getChannelCount() const -> std::uint32_t { return mNumChannels; }
    [[nodiscard]] auto getInputChannel(size_t index) const -> std::uint8_t { return mChannels.at(index); }
    [[nodiscard]] auto getOutputChannel(size_t index) const -> ChannelPan { return mOutputAttenuationChannels.at(index); }

private:
    std::array<std::uint8_t, cMaxChannelsPerTrack> mChannels{};
    std::array<ChannelPan, cMaxChannelsPerTrack> mOutputAttenuationChannels{};
    std::uint32_t mNumChannels = 0;
};

struct UnknownMetadataPoint {
    std::uint32_t samplePos;
    float _04;
};

struct MetadataPointInfo {
    std::vector<UnknownMetadataPoint> points;
    std::uint16_t _01;
};

struct OptionalMetadata {
    std::optional<float> maxAmplitude;
    std::optional<float> _01;
    std::optional<float> maxMomentaryLufs;
    std::optional<float> integratedLufs;
    std::optional<std::uint32_t> tailLength;
    std::optional<MetadataPointInfo> _05;
    std::optional<float> _06;
    std::optional<std::uint32_t> _07;
};

struct Marker {
    std::uint32_t id;
    std::uint32_t start;
    std::uint32_t duration;
    std::string name;
};

class Metadata {
public:
    Metadata() = default;

    auto setIsStreaming(bool b) -> void { mIsStreaming = b; }
    [[nodiscard]] auto getIsStreaming() const -> bool { return mIsStreaming; }
    auto setHasSoundInArchive(bool b) -> void { mHasSoundInArchive = b; }
    [[nodiscard]] auto getHasSoundInArchive() const -> bool { return mHasSoundInArchive; }
    auto setIsLoop(bool b) -> void { mIsLoop = b; }
    [[nodiscard]] auto getIsLoop() const -> bool { return mIsLoop; }
    auto setIsOpus(bool b) -> void { mIsOpus = b; }
    [[nodiscard]] auto getIsOpus() const -> bool { return mIsOpus; }
    auto setIsBatchOpusDecode(bool b) -> void { mIsBatchOpusDecode = b; }
    [[nodiscard]] auto getIsBatchOpusDecode() const -> bool { return mIsBatchOpusDecode; }
    auto setUniformTrackAttenutation(bool b) -> void { mUniformTrackAttenutation = b; }
    [[nodiscard]] auto getUniformTrackAttenutation() const -> bool { return mUniformTrackAttenutation; }
    auto setIsPublic(bool b) -> void { mIsPublic = b; }
    [[nodiscard]] auto getIsPublic() const -> bool { return mIsPublic; }

    auto setName(std::string_view name) -> void { mName = name; }
    [[nodiscard]] auto getName() const -> std::string_view { return mName; }

    // TODO: this sucks
    auto addTrack() -> void { if (mTrackCount < cMaxTracks) { ++mTrackCount; } }
    [[nodiscard]] auto getTrackCount() const -> std::uint32_t { return mTrackCount; }
    [[nodiscard]] auto getTrack(size_t index) -> Track& { return mTracks.at(index); }
    [[nodiscard]] auto getTrack(size_t index) const -> const Track& { return mTracks.at(index); }

    template <typename... Args>
    auto initOptData(Args&&... args) -> void { mOptionalMetadata = std::make_optional<OptionalMetadata>(std::forward<Args>(args)...); }
    [[nodiscard]] auto getOptData() -> std::optional<OptionalMetadata>& { return mOptionalMetadata; }
    [[nodiscard]] auto getOptData() const -> const std::optional<OptionalMetadata>& { return mOptionalMetadata; }

    template <typename... Args>
    auto addMarker(Args&&... args) -> Marker& { return mMarkers.emplace_back(std::forward<Args>(args)...); }
    [[nodiscard]] auto getMarkerCount() const -> std::uint32_t { return mMarkers.size(); }
    [[nodiscard]] auto getMarkers() -> std::vector<Marker>& { return mMarkers; }
    [[nodiscard]] auto getMarkers() const -> const std::vector<Marker>& { return mMarkers; }

    auto addTag(std::string_view name) -> void { mTags.emplace_back(name); }
    [[nodiscard]] auto getTagCount() const -> std::uint32_t { return mTags.size(); }
    [[nodiscard]] auto getTags() -> std::vector<std::string>& { return mTags; }
    [[nodiscard]] auto getTags() const -> const std::vector<std::string>& { return mTags; }

    template <typename... Args>
    auto initMusicInfo(Args&&... args) -> void { mMusicInfo = std::make_optional<MusicInfo>(std::forward<Args>(args)...); }
    [[nodiscard]] auto getMusicInfo() -> std::optional<MusicInfo>&  { return mMusicInfo; }
    [[nodiscard]] auto getMusicInfo() const -> const std::optional<MusicInfo>&  { return mMusicInfo; }

    auto setEndian(std::endian endian) -> void { mEndian = endian; }
    [[nodiscard]] auto getEndian() const -> std::endian { return mEndian; }

    auto reset() -> void;

    auto calculate(const sound::Sound& sound) -> void;

    auto dumpMetadata() const -> std::string;

private:
    std::string mName;
    std::array<Track, cMaxTracks> mTracks;
    std::uint32_t mTrackCount;
    std::optional<OptionalMetadata> mOptionalMetadata;
    std::optional<MusicInfo> mMusicInfo;
    std::vector<Marker> mMarkers;
    std::vector<std::string> mTags;
    bool mIsStreaming;
    bool mHasSoundInArchive; // has non-empty BWAV file in archive
    bool mIsLoop;
    bool mIsOpus;
    bool mIsBatchOpusDecode; // decode four frames at once
    bool mUniformTrackAttenutation; // all channels per track will receive the same output attenuation
    bool mIsPublic;
    std::endian mEndian;
};

} // namespace sound