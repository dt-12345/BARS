#pragma once

namespace decode {

enum Result {
    OK,
    FileNotFound,
    BufferTooSmall,
    InvalidBarsMagic,
    InvalidAmtaMagic,
    InvalidMinfMagic,
    InvalidBwavMagic,
    InvalidWavMagic,
    InvalidBarsVersion,
    InvalidAmtaVersion,
    InvalidMinfVersion,
    InvalidBwavVersion,
    InvalidAssetType,
    InvalidFormat,
    CannotDetermineOpusSize,
    TooManyTracks,
    TooManyChannels,
    InvalidWavChunk,
    MissingWavChunk,
    UnsupportedWavFormat,
};

inline constexpr auto ToString(Result res) -> const char* {
    switch (res) {
        case Result::OK: return "OK";
        case Result::FileNotFound: return "FileNotFound";
        case Result::BufferTooSmall: return "BufferTooSmall";
        case Result::InvalidBarsMagic: return "InvalidBarsMagic";
        case Result::InvalidAmtaMagic: return "InvalidAmtaMagic";
        case Result::InvalidMinfMagic: return "InvalidMinfMagic";
        case Result::InvalidBwavMagic: return "InvalidBwavMagic";
        case Result::InvalidWavMagic: return "InvalidWavMagic";
        case Result::InvalidBarsVersion: return "InvalidBarsVersion";
        case Result::InvalidAmtaVersion: return "InvalidAmtaVersion";
        case Result::InvalidMinfVersion: return "InvalidMinfVersion";
        case Result::InvalidBwavVersion: return "InvalidBwavVersion";
        case Result::InvalidAssetType: return "InvalidAssetType";
        case Result::InvalidFormat: return "InvalidFormat";
        case Result::CannotDetermineOpusSize: return "CannotDetermineOpusSize";
        case Result::TooManyTracks: return "TooManyTracks";
        case Result::TooManyChannels: return "TooManyChannels";
        case Result::InvalidWavChunk: return "InvalidWavChunk";
        case Result::MissingWavChunk: return "MissingWavChunk";
        case Result::UnsupportedWavFormat: return "UnsupportedWavFormat";
        default: return "UNKNOWN";
    }
}

} // namespace decode