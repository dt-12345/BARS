#pragma once

#include <bit>
#include <cstdint>

namespace resource {

constexpr const auto cAdpcmSamplesPerFrame = 14u;

struct AdpcmContext {
    std::uint16_t predictorScale;
    std::int16_t history[2];

    auto getPredictor() const -> std::uint32_t {
        return static_cast<std::uint32_t>((predictorScale & 0xff) >> 4);
    }

    auto getScale() const -> std::int32_t {
        return 1 << (predictorScale & 0xf);
    }
};

struct AdpcmParameter {
    std::int16_t coefficients[8][2];

    auto byteswap() -> void {
        for (size_t i = 0; i < 8; ++i) {
            for (size_t j = 0; j < 2; ++j) {
                coefficients[i][j] = std::byteswap(coefficients[i][j]);
            }
        }
    }
};

[[nodiscard]]
constexpr inline auto CalcAdpcmSampleByteSize(std::uint32_t numSamples) -> std::uint32_t {
    const auto remainder = numSamples % 14;
    const auto extra = remainder ? (remainder / 2 + (numSamples & 1) + 1) : 0u;
    return numSamples / 14 * 8 + extra;
}

[[nodiscard]]
constexpr inline auto CalcAdpcmSampleCountFromByteSize(std::uint32_t byteSize) -> std::uint32_t {
    return byteSize / 8 * cAdpcmSamplesPerFrame + (byteSize % 8 ? (byteSize % 8 - 1) * 2 : 0);
}

} // namespace resource