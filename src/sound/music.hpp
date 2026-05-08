#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
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

using Note = std::uint8_t;

struct Chord {
    std::uint32_t samplePos;
    std::vector<Note> notes;
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

namespace thunder {

struct __attribute__((packed)) ResNote {
    std::uint32_t start;
    std::uint32_t end;
    std::uint16_t pitch;
    std::uint16_t velocity;
    std::uint16_t _0c;
};

struct ResSequence2 {
    std::uint32_t _00;
    std::uint32_t _04;
};

struct __attribute__((packed)) ResSequence3 {
    std::uint32_t samplePosition;
    std::uint16_t _04;
};

struct SequenceTrack {
    std::string name;
    std::vector<ResNote> notes;
    std::vector<ResSequence2> _02;
    std::vector<ResSequence3> _03;
};

using SequenceTable = std::vector<thunder::SequenceTrack>;

} // namespace thunder

namespace park {

enum VoiceType : std::uint8_t {
    Undefined = 0,
    None = 1,
    A = 2,
    I = 3,
    U = 4,
    E = 5,
    O = 6,
    ILow = 7,
    OLow = 8,
    Humming = 9,
    Whistle = 10,
    Shout = 11,
    _12 = 12, // ??? パクティク
};

// these are probably just hashes but I don't know what the original strings are
enum GuitarAction : std::uint32_t {
    Normal = 0xd2166605,
    Muted = 0x282e797a,
};

struct ResNote {
    std::uint32_t start;
    std::uint32_t duration;
    std::array<std::uint8_t, 4> data; // type, pitch, velocity for voice / pitch, velocity for everything else
};

struct ResGuitarNote {
    GuitarAction action;
    ResNote note;
};

struct ResPitchValue {
    std::uint32_t samplePosition;
    std::int32_t value;
};

struct VocalNoteTable {
    std::vector<ResNote> notes;
    std::int32_t _04;
};

struct PitchValueTable {
    std::vector<ResPitchValue> values;
    std::int32_t _04;
};

struct GenericMusic {
    VocalNoteTable vocals;
    std::vector<ResGuitarNote> guitar;
    std::vector<PitchValueTable> pitchBends;
    std::vector<PitchValueTable> vibrato;
};

struct DJMusic {
    VocalNoteTable vocals;
    std::vector<ResGuitarNote> guitar;
    std::vector<ResNote> rhythm;
    std::vector<ResNote> bass;
    std::vector<ResNote> piano;
    std::vector<ResNote> synth;
    std::vector<ResNote> break_;
};

} // namespace park

using SequenceData = std::variant<
    std::unique_ptr<thunder::SequenceTable>,
    std::unique_ptr<park::GenericMusic>,
    std::unique_ptr<park::DJMusic>
>;

class MusicInfo {
public:
    MusicInfo();

    auto setTrackName(std::string_view name) -> void { mTrackName = name; }
    [[nodiscard]] auto getTrackName() const -> std::string_view { return mTrackName; }
    auto setJapaneseName(std::string_view name) -> void { mJapaneseName = name; }
    [[nodiscard]] auto getJapaneseName() const -> std::string_view { return mJapaneseName; }

    auto setSampleRate(std::uint32_t sampleRate) -> void { mSampleRate = sampleRate; }
    [[nodiscard]] auto getSampleRate() const -> std::uint32_t { return mSampleRate; }

    auto setLoopEnd(std::uint32_t loopEnd) -> void { mLoopEnd = loopEnd; }
    [[nodiscard]] auto getLoopEnd() const -> std::uint32_t { return mLoopEnd; }

    auto setLoopStart(std::uint32_t loopStart) -> void { mLoopStart = loopStart; }
    [[nodiscard]] auto getLoopStart() const -> std::uint32_t { return mLoopStart; }

    auto setDefaultTempoMeter(const TempoMeter& tempoMeter) -> void { mDefaultTempoMeter = tempoMeter; }
    [[nodiscard]] auto getDefaultTempoMeter() -> TempoMeter& { return mDefaultTempoMeter; }
    [[nodiscard]] auto getDefaultTempoMeter() const -> const TempoMeter& { return mDefaultTempoMeter; }

    auto initTempoMeterTable() -> void { mTempoMeters = std::make_optional<Table<TempoMeter>>(); }
    [[nodiscard]] auto getTempoMeterTable() -> std::optional<Table<TempoMeter>>& { return mTempoMeters; }
    [[nodiscard]] auto getTempoMeterTable() const -> const std::optional<Table<TempoMeter>>& { return mTempoMeters; }

    auto initChordTable() -> void { mChords = std::make_optional<Table<Chord>>(); }
    [[nodiscard]] auto getChordTable() -> std::optional<Table<Chord>>& { return mChords; }
    [[nodiscard]] auto getChordTable() const -> const std::optional<Table<Chord>>& { return mChords; }
    
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

    auto initS3SequenceData() -> void { mSequenceData = std::make_optional<SequenceData>(std::make_unique<thunder::SequenceTable>()); }
    auto initACNHSequenceData() -> void { mSequenceData = std::make_optional<SequenceData>(std::make_unique<park::GenericMusic>()); }
    auto initACNHDJSequenceData() -> void { mSequenceData = std::make_optional<SequenceData>(std::make_unique<park::DJMusic>()); }
    [[nodiscard]] auto getSequenceData() -> std::optional<SequenceData>& { return mSequenceData; }
    [[nodiscard]] auto getSequenceData() const -> const std::optional<SequenceData>& { return mSequenceData; }

    auto setEndian(std::endian endian) -> void { mEndian = endian; }
    [[nodiscard]] auto getEndian() const -> std::endian { return mEndian; }

private:
    std::string mTrackName;
    std::string mJapaneseName;
    std::uint32_t mSampleRate;
    std::uint32_t mLoopStart;
    std::uint32_t mLoopEnd;
    TempoMeter mDefaultTempoMeter;
    std::optional<Table<TempoMeter>> mTempoMeters;
    std::optional<Table<Chord>> mChords;
    std::optional<Table<Beat>> mBeats;
    std::optional<Table<std::uint32_t>> mMeasures;
    std::optional<std::vector<PointMarker>> mPointMarkers;
    std::optional<std::vector<RangeMarker>> mRangeMarkers;
    std::optional<SequenceData> mSequenceData;
    std::endian mEndian;
};

} // namespace sound