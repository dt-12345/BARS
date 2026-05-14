#pragma once

#include "sound/channel.hpp"

namespace sound {

class Sound {
public:
    Sound() : mChannels(), mAssetType(AssetType::Invalid), mEndian(std::endian::native) {}
    explicit Sound(AssetType assetType, std::endian endian = std::endian::native) : mChannels(), mAssetType(assetType), mEndian(endian) {}

    auto setChannelCount(size_t numChannels) -> void;
    [[nodiscard]] auto getChannelCount() const -> size_t { return mChannels.size(); }

    [[nodiscard]] auto getSampleRate() const -> std::uint32_t { return getChannelCount() > 0 ? getChannels()[0].getSampleRate() : 0; }
    [[nodiscard]] auto getSampleCount() const -> std::uint32_t { return getChannelCount() > 0 ? getChannels()[0].getSampleCount() : 0; }
    [[nodiscard]] auto getSampleFormat() const -> Format { return getChannelCount() > 0 ? getChannels()[0].getSampleFormat() : Format::PcmInt16; }
    [[nodiscard]] auto getLoopEnd() const -> std::int32_t { return getChannelCount() > 0 ? getChannels()[0].getLoopEnd() : -1; }
    [[nodiscard]] auto getDuration() const -> float { return getSampleRate() > 0 ? static_cast<float>(getSampleCount()) / static_cast<float>(getSampleRate()) : 0.f; }

    [[nodiscard]] auto isPrefetch() const -> bool { return mAssetType == AssetType::Prefetch; }
    [[nodiscard]] auto createPrefetch(Sound& prefetch) const -> bool;
    [[nodiscard]] auto requiresPrefetch() const -> bool;
    auto setPrefetchInfo(const Sound& prefetch) -> void;

    [[nodiscard]] auto getChannels() -> std::vector<Channel>& { return mChannels; }
    [[nodiscard]] auto getChannels() const -> const std::vector<Channel>& { return mChannels; }

    auto setAssetType(AssetType type) -> void { mAssetType = type; }
    [[nodiscard]] auto getAssetType() const -> AssetType { return mAssetType; }

    auto setEndian(std::endian endian) -> void { mEndian = endian; }
    [[nodiscard]] auto getEndian() const -> std::endian { return mEndian; }

    auto setLoopPoint(std::uint32_t loopPoint) -> void { for (auto& channel : mChannels) { channel.setLoopPoint(loopPoint); } }

    auto convert(Format fmt) -> bool;
    auto resample(std::uint32_t sampleRate) -> bool;
    auto setSampleCount(std::uint32_t sampleCount) -> bool;
    auto extend(std::uint32_t additionalSamples) -> bool;
    auto cut(std::uint32_t samples) -> bool;
    auto alignUp(std::uint32_t align) -> bool;
    auto resampleAndConvert(std::endian endian, Format fmt, std::uint32_t sampleRate, std::uint32_t sampleCount = 0xffff'ffff) -> bool;
    auto resampleAndConvert(Format fmt, std::uint32_t sampleRate, std::uint32_t sampleCount = 0xffff'ffff) -> bool { return resampleAndConvert(mEndian, fmt, sampleRate, sampleCount); }
    
    auto reset() -> void { mChannels.clear(); mAssetType = AssetType::Invalid; mEndian = std::endian::native; }

    auto calcHash() const -> std::uint32_t;

private:
    std::vector<Channel> mChannels;
    AssetType mAssetType;
    std::endian mEndian;
};

} // namespace sound