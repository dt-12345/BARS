#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sound {

template <typename T>
struct Table {
    std::int16_t loopBaseIndex;
    std::vector<T> entries;
};

struct TimeSignature {
    std::uint16_t upper, lower;
};

struct TempoMeter {
    std::uint32_t samplePos;
    float tempo;
    TimeSignature timeSignature;
};

struct Beat {
    std::uint32_t samplePos;
    std::uint32_t beatNum; // which beat in the measure is this
};

struct PointMarker {
    std::string name;
    std::int16_t loopBaseIndex;
    std::vector<std::uint32_t> samplePositions;
};

struct RangeMarkerPoint {
    std::uint32_t samplePos;
    std::uint32_t _04;
};

struct RangeMarker {
    std::string name;
    Table<RangeMarkerPoint> starts;
    Table<RangeMarkerPoint> ends;
};

class MusicInfo {
public:
    MusicInfo();

    auto setTrackName(std::string_view name) -> void { mTrackName = name; }
    [[nodiscard]] auto getTrackName() const -> std::string_view { return mTrackName; }
    auto setJapaneseName(std::string_view name) -> void { mJapaneseName = name; }
    [[nodiscard]] auto getJapaneseName() const -> std::string_view { return mJapaneseName; }

    auto setSampleRate(std::uint32_t sampleRate) -> void { mSampleRate = sampleRate; }
    [[nodiscard]] auto getSampleRate() const -> std::uint32_t { return mSampleRate; }

    auto setSampleCount(std::uint32_t sampleCount) -> void { mSampleCount = sampleCount; }
    [[nodiscard]] auto getSampleCount() const -> std::uint32_t { return mSampleCount; }

    auto setLoopStart(std::uint32_t loopStart) -> void { mLoopStart = loopStart; }
    [[nodiscard]] auto getLoopStart() const -> std::uint32_t { return mLoopStart; }

    auto setDefaultTempoMeter(const TempoMeter& tempoMeter) -> void { mDefaultTempoMeter = tempoMeter; }
    [[nodiscard]] auto getDefaultTempoMeter() -> TempoMeter& { return mDefaultTempoMeter; }
    [[nodiscard]] auto getDefaultTempoMeter() const -> const TempoMeter& { return mDefaultTempoMeter; }

    auto initTempoMeterTable() -> void { mTempoMeters = std::make_optional<Table<TempoMeter>>(); }
    [[nodiscard]] auto getTempoMeterTable() -> std::optional<Table<TempoMeter>>& { return mTempoMeters; }
    [[nodiscard]] auto getTempoMeterTable() const -> const std::optional<Table<TempoMeter>>& { return mTempoMeters; }
    
    auto initBeatTable() -> void { mBeats = std::make_optional<Table<Beat>>(); }
    [[nodiscard]] auto getBeatTable() -> std::optional<Table<Beat>>& { return mBeats; }
    [[nodiscard]] auto getBeatTable() const -> const std::optional<Table<Beat>>& { return mBeats; }
    
    auto initMeasureTable() -> void { mMeasures = std::make_optional<Table<std::uint32_t>>(); }
    [[nodiscard]] auto getMeasureTable() -> std::optional<Table<std::uint32_t>>& { return mMeasures; }
    [[nodiscard]] auto getMeasureTable() const -> const std::optional<Table<std::uint32_t>>& { return mMeasures; }
    
    auto initPointMarkerTable() -> void { mPointMarkers = std::make_optional<std::vector<PointMarker>>(); }
    [[nodiscard]] auto getPointMarkerTable() -> std::optional<std::vector<PointMarker>>& { return mPointMarkers; }
    [[nodiscard]] auto getPointMarkerTable() const -> const std::optional<std::vector<PointMarker>>& { return mPointMarkers; }
    
    auto initRangeMarkerTable() -> void { mRangeMarkers = std::make_optional<std::vector<RangeMarker>>(); }
    [[nodiscard]] auto getRangeMarkerTable() -> std::optional<std::vector<RangeMarker>>& { return mRangeMarkers; }
    [[nodiscard]] auto getRangeMarkerTable() const -> const std::optional<std::vector<RangeMarker>>& { return mRangeMarkers; }

    auto setEndian(std::endian endian) -> void { mEndian = endian; }
    [[nodiscard]] auto getEndian() const -> std::endian { return mEndian; }

private:
    std::string mTrackName;
    std::string mJapaneseName;
    std::uint32_t mSampleRate;
    std::uint32_t mSampleCount;
    std::uint32_t mLoopStart;
    TempoMeter mDefaultTempoMeter;
    std::optional<Table<TempoMeter>> mTempoMeters;
    std::optional<Table<Beat>> mBeats;
    std::optional<Table<std::uint32_t>> mMeasures;
    std::optional<std::vector<PointMarker>> mPointMarkers;
    std::optional<std::vector<RangeMarker>> mRangeMarkers;
    std::endian mEndian;
};

} // namespace sound