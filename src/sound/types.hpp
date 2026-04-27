#pragma once

#include <cstdint>

namespace sound {

enum class AssetType : std::uint32_t {
    Normal,
    Prefetch,
    Invalid = 0xffff'ffff,
};

enum class Format : std::uint32_t {
    PcmInt16,
    DspAdpcm,
    Opus,
};

enum class ChannelPan : std::uint32_t {
    Left,
    Right,
    Center,
    LowFrequencyEffect,
    LeftSurround,
    RightSurround,
};

inline constexpr auto IsWritableFormat(Format fmt) -> bool {
    return fmt == Format::PcmInt16 || fmt == Format::DspAdpcm || fmt == Format::Opus;
}

inline constexpr auto ToString(AssetType type) -> const char* {
    switch (type) {
        case AssetType::Normal: return "Normal";
        case AssetType::Prefetch: return "Prefetch";
        default: return "Invalid";
    }
}

inline constexpr auto ToString(Format type) -> const char* {
    switch (type) {
        case Format::PcmInt16: return "PcmInt16";
        case Format::DspAdpcm: return "DspAdpcm";
        case Format::Opus: return "Opus";
        default: return "Unknown";
    }
}

inline constexpr auto ToString(ChannelPan ch) -> const char* {
    switch (ch) {
        case ChannelPan::Left: return "Left";
        case ChannelPan::Right: return "Right";
        case ChannelPan::Center: return "Center";
        case ChannelPan::LowFrequencyEffect: return "LowFrequencyEffect";
        case ChannelPan::LeftSurround: return "LeftSurround";
        case ChannelPan::RightSurround: return "RightSurround";
        default: return "Unknown";
    }
}

} // namespace sound