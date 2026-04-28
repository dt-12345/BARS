#include "sound/metadata.hpp"

#include <format>
#include <ranges>

namespace sound {

auto Track::addChannel(std::uint8_t inputIndex, ChannelPan outputPan) -> bool {
    if (mNumChannels >= cMaxChannelsPerTrack) {
        return false;
    }

    mChannels[mNumChannels] = inputIndex;
    mOutputAttenuationChannels[mNumChannels++] = outputPan;

    return true;
}

auto Metadata::reset() -> void {
    mName = "";
    mTrackCount = 0;
    mOptionalMetadata.reset();
    mMusicInfo.reset();
    mMarkers.clear();
    mTags.clear();
    mIsStreaming = false;
    mHasSoundInArchive = false;
    mIsLoop = false;
    mIsOpus = false;
    mIsBatchOpusDecode = false;
    mUniformTrackAttenutation = false;
    mIsPublic = false;
    mEndian = std::endian::native;
}

auto Metadata::calculate(const sound::Sound& sound) -> void {
    mIsLoop = sound.getLoopEnd() != -1;
    mIsOpus = sound.getSampleFormat() == Format::Opus;
    if (mIsOpus) {
        mIsBatchOpusDecode = true;
    }
    mEndian = sound.getEndian();

    // TODO: handle multi-track bwavs
    if (sound.getChannelCount() <= 6 && mTrackCount < cMaxTracks) {
        auto& track = mTracks[mTrackCount++];
        for (const auto& [i, channel] : sound.getChannels() | std::views::enumerate) {
            track.addChannel(i, channel.getOutputChannel());
        }
    }

    // TODO: other stuff?
}

auto Metadata::dumpMetadata() const -> std::string {
    auto out = std::string{};
    out += std::format("{}:\n", mName);

    out += std::format("  IsStreaming: {}\n", mIsStreaming);
    out += std::format("  HasSoundInArchive: {}\n", mHasSoundInArchive);
    out += std::format("  IsLoop: {}\n", mIsLoop);
    out += std::format("  IsOpus: {}\n", mIsOpus);
    out += std::format("  IsBatchOpusDecode: {}\n", mIsBatchOpusDecode);
    out += std::format("  UniformTrackAttenutation: {}\n", mUniformTrackAttenutation);
    out += std::format("  IsPublic: {}\n", mIsPublic);

    if (mTrackCount > 0) {
        out += "  Tracks:\n";
        for (const auto i : std::views::iota(0u, mTrackCount)) {
            const auto& track = getTrack(i);
            for (const auto j : std::views::iota(0u, track.getChannelCount())) {
                out += std::format("  - Channel Index: {}\n    Output Pan: {}\n", track.getInputChannel(j), ToString(track.getOutputChannel(j)));
            }
        }
    }

    if (mOptionalMetadata) {
        if (mOptionalMetadata->maxAmplitude) {
            out += std::format("  Max Amplitude: {}\n", *mOptionalMetadata->maxAmplitude);
        }
        if (mOptionalMetadata->_01) {
            out += std::format("  Unknown 1: {}\n", *mOptionalMetadata->_01);
        }
        if (mOptionalMetadata->maxMomentaryLufs) {
            out += std::format("  Max Momentary LUFS: {}\n", *mOptionalMetadata->maxMomentaryLufs);
        }
        if (mOptionalMetadata->integratedLufs) {
            out += std::format("  Integrated LUFS: {}\n", *mOptionalMetadata->integratedLufs);
        }
        if (mOptionalMetadata->tailLength) {
            out += std::format("  Tail Length: {}\n", *mOptionalMetadata->tailLength);
        }
        if (mOptionalMetadata->_05) {
            out += "  Unknown 5:\n";
            out += std::format("    Unknown: {}\n", mOptionalMetadata->_05->_01);
            if (mOptionalMetadata->_05->points.size() > 0) {
                out += "    Points:\n";
                for (const auto& point : mOptionalMetadata->_05->points) {
                    out += std::format("      {{ Sample Position: {}, Unknown: {} }}\n", point.samplePos, point._04);
                }
            }
        }
        if (mOptionalMetadata->_06) {
            out += std::format("  Unknown 6: {}\n", *mOptionalMetadata->_06);
        }
        if (mOptionalMetadata->_07) {
            out += std::format("  Unknown 7: {}\n", *mOptionalMetadata->_07);
        }
    }

    if (mTags.size() > 0) {
        out += "  Tags:\n";
        for (const auto& tag : mTags) {
            out += std::format("  - {}\n", tag);
        }
    }

    if (mMarkers.size() > 0) {
        out += "  Markers:\n";
        for (const auto& marker : mMarkers) {
            out += std::format("    {}: {{ ID: {}, Start Sample: {}, Duration: {} }}\n", marker.name, marker.id, marker.start, marker.duration);
        }
    }

    if (mMusicInfo) {
        out += "  Music Info:\n";
        if (mMusicInfo->getTempoMeterTable()) {
            out += "    Tempo and Meter:\n";
            for (const auto& tm : mMusicInfo->getTempoMeterTable()->entries) {
                out += std::format(
                    "    {}: {{ Tempo: {}, Time Signature: {}/{} }}\n",
                    tm.samplePos, tm.tempo, tm.timeSignature.upper, tm.timeSignature.lower
                );
            }
        }
        if (mMusicInfo->getBeatTable()) {
            out += "    Beats:\n";
            for (const auto& beat : mMusicInfo->getBeatTable()->entries) {
                out += std::format("      {}: {}\n", beat.samplePos, beat.beatNum);
            }
        }
        if (mMusicInfo->getMeasureTable()) {
            out += "    Measures:\n";
            for (const auto& measure : mMusicInfo->getMeasureTable()->entries) {
                out += std::format("    - {}\n", measure);
            }
        }
        if (mMusicInfo->getPointMarkerTable()) {
            out += "    Point Markers:\n";
            for (const auto& marker : *mMusicInfo->getPointMarkerTable()) {
                out += std::format("      {}:\n", marker.name);
                for (const auto& pos : marker.samplePositions) {
                    out += std::format("      - {}\n", pos);
                }
            }
        }
        if (mMusicInfo->getRangeMarkerTable()) {
            for (const auto& marker : *mMusicInfo->getRangeMarkerTable()) {
                out += std::format("      {}:\n", marker.name);
                for (const auto& [start, end] : std::views::zip(marker.starts.entries, marker.ends.entries)) {
                    out += std::format("      - {{ Start: {}, End: {} }}\n", start.samplePos, end.samplePos);
                }
            }
        }
    }

    return out;
}

} // namespace sound