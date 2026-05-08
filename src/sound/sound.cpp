#include "common/hash.hpp"
#include "sound/sound.hpp"

#include <atomic>
#include <execution>
#include <ranges>

namespace sound {

auto Sound::setChannelCount(size_t numChannels) -> void {
    if (mChannels.size() < numChannels) {
        mChannels.resize(numChannels);
    } else if (mChannels.size() > numChannels) {
        mChannels.erase(mChannels.begin() + numChannels, mChannels.end());
    }
}

auto Sound::createPrefetch(Sound& prefetch) const -> bool {
    if (mAssetType == AssetType::Prefetch) {
        return false;
    }

    prefetch.mEndian = mEndian;
    prefetch.mAssetType = AssetType::Prefetch;
    prefetch.mChannels.resize(getChannelCount());
    for (const auto& [channel, prefetch] : std::views::zip(std::as_const(mChannels), prefetch.mChannels)) {
        if (!channel.createPrefetch(prefetch, mEndian)) {
            return false;
        }
    }

    return true;
}

auto Sound::requiresPrefetch() const -> bool {
    return std::any_of(mChannels.begin(), mChannels.end(), [](const auto& channel) { return channel.requiresStreaming(); });
}

auto Sound::setPrefetchInfo(const Sound& prefetch) -> void {
    for (const auto& [channel, prefetch] : std::views::zip(mChannels, std::as_const(prefetch.mChannels))) {
        channel.setPrefetchInfo(prefetch);
    }
}

auto Sound::convert(Format fmt) -> bool {
    if (mAssetType == AssetType::Prefetch) {
        return false;
    }

    const auto endian = mEndian;
    auto success = std::atomic<std::uint32_t>{ 0 };
    std::for_each(
        std::execution::par_unseq, mChannels.begin(), mChannels.end(),
        [=, &success](auto&& channel) {
            if (channel.convert(fmt, endian)) {
                success++;
            }
        }
    );

    return success == getChannelCount();
}

auto Sound::resample(std::uint32_t sampleRate) -> bool {
    if (mAssetType == AssetType::Prefetch) {
        return false;
    }

    const auto endian = mEndian;
    auto success = std::atomic<std::uint32_t>{ 0 };
    std::for_each(
        std::execution::par_unseq, mChannels.begin(), mChannels.end(),
        [=, &success](auto&& channel) {
            if (channel.resample(sampleRate, endian)) {
                success++;
            }
        }
    );

    return success == getChannelCount();
}

auto Sound::setSampleCount(std::uint32_t sampleCount) -> bool {
    if (mAssetType == AssetType::Prefetch) {
        return false;
    }

    const auto endian = mEndian;
    auto success = std::atomic<std::uint32_t>{ 0 };
    std::for_each(
        std::execution::par_unseq, mChannels.begin(), mChannels.end(),
        [=, &success](auto&& channel) {
            if (channel.setSampleCount(sampleCount, endian)) {
                success++;
            }
        }
    );

    return success == getChannelCount();
}

auto Sound::extend(std::uint32_t additionalSamples) -> bool {
    if (mAssetType == AssetType::Prefetch) {
        return false;
    }

    const auto endian = mEndian;
    auto success = std::atomic<std::uint32_t>{ 0 };
    std::for_each(
        std::execution::par_unseq, mChannels.begin(), mChannels.end(),
        [=, &success](auto&& channel) {
            if (channel.extend(additionalSamples, endian)) {
                success++;
            }
        }
    );

    return success == getChannelCount();
}

auto Sound::cut(std::uint32_t samples) -> bool {
    if (mAssetType == AssetType::Prefetch) {
        return false;
    }

    const auto endian = mEndian;
    auto success = std::atomic<std::uint32_t>{ 0 };
    std::for_each(
        std::execution::par_unseq, mChannels.begin(), mChannels.end(),
        [=, &success](auto&& channel) {
            if (channel.cut(samples, endian)) {
                success++;
            }
        }
    );

    return success == getChannelCount();
}

auto Sound::alignUp(std::uint32_t align) -> bool {
    if (mAssetType == AssetType::Prefetch) {
        return false;
    }

    const auto endian = mEndian;
    auto success = std::atomic<std::uint32_t>{ 0 };
    std::for_each(
        std::execution::par_unseq, mChannels.begin(), mChannels.end(),
        [=, &success](auto&& channel) {
            if (channel.alignUp(align, endian)) {
                success++;
            }
        }
    );

    return success == getChannelCount();
}

auto Sound::resampleAndConvert(std::endian endian, Format fmt, std::uint32_t sampleRate, std::uint32_t sampleCount) -> bool {
    if (mAssetType == AssetType::Prefetch) {
        return false;
    }

    auto success = std::atomic<std::uint32_t>{ 0 };
    std::for_each(
        std::execution::par_unseq, mChannels.begin(), mChannels.end(),
        [=, &success](auto&& channel) {
            if (channel.resampleAndConvert(fmt, sampleRate, sampleCount == 0xffff'ffff ? channel.getSampleCount() : sampleCount, endian)) {
                success++;
            }
        }
    );

    if (success == getChannelCount()) {
        mEndian = endian;
        return true;
    }

    return false;
}

auto Sound::calcHash() const -> std::uint32_t {
    auto ctx = common::CRC32Context{};
    for (const auto& channel : mChannels) {
        const auto& data = channel.getRawSampleData();
        ctx.update(data.data(), data.size());
    }
    return ctx.get();
}

} // namespace sound